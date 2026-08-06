#include <adapter.hpp>
#include <algorithm>
#include <chrono>
#include <config.hpp>
#include <fstream>
#include <iterator>
#include <macro.hpp>
#include <macro_manager.hpp>
#include <polkit_utils.hpp>
#include <script_binding.hpp>
#include <stdexcept>
#include <thread>
#include <toml.hpp>
#include <ui.hpp>
#include <utils.hpp>
extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}
#include "LuaBridge/LuaBridge.h"

namespace {
// Pre-parse every enabled hotkey's Lua script into a cached macro_script.
// Scripts are executed ONCE at startup; at runtime only the cached
// command arrays run (no Lua).
void warmup_macros() {
    lua_State* L = luaL_newstate();
    if (!L) {
        log_msg("moused: failed to create lua state\n");
        return;
    }
    luaL_openlibs(L);
    register_script_enums(L);

    for (const auto& prop : keys_properties) {
        if (!prop.enabled) continue;

        std::string script;
        if (prop.type == script_type::in_line) {
            script = prop.code;
        } else if (prop.type == script_type::file) {
            std::ifstream f(prop.code);
            if (!f) {
                log_msg("moused: cannot open script file `%s`\n",
                        prop.code.c_str());
                continue;
            }
            script.assign(std::istreambuf_iterator<char>(f),
                          std::istreambuf_iterator<char>());
        } else {
            log_msg("moused: unknown script type for key %d\n",
                    static_cast<int>(prop.keys.front()));
            continue;
        }

        if (luaL_loadbuffer(L, script.data(), script.size(), "hotkey") !=
                LUA_OK ||
            lua_pcall(L, 0, 0, 0) != LUA_OK) {
            log_msg("moused: error loading script for key %d: %s\n",
                    static_cast<int>(prop.keys.front()), lua_tostring(L, -1));
            lua_pop(L, 1);
            continue;
        }

        lua_getglobal(L, "run");
        if (!lua_isfunction(L, -1)) {
            log_msg("moused: script for key %d has no `run()`\n",
                    static_cast<int>(prop.keys.front()));
            lua_pop(L, 1);
            continue;
        }
        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            log_msg("moused: run() failed for key %d: %s\n",
                    static_cast<int>(prop.keys.front()), lua_tostring(L, -1));
            lua_pop(L, 1);
            continue;
        }

        try {
            macro_script ms = parse_lua_result(L);
            macro::register_macro(prop.keys, ms, prop.loop);
        } catch (const std::exception& e) {
            log_msg("moused: parse failed for key %d: %s\n",
                    static_cast<int>(prop.keys.front()), e.what());
        }
        lua_settop(L, 0);
    }

    lua_close(L);
}

// Background listener: polls the keyboard and toggles macros on the
// rising edge of a registered hotkey press.
std::jthread g_listener;
}  // namespace

bool moused::OnInit() {
    try {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__)
        // Elevation happens in main() BEFORE wxEntry/GTK init: the pkexec
        // re-exec'd root process needs the display env restored from the
        // envfile, and gtk_init_check() runs before OnInit().
        // Open /dev/uinput while we are still root — the fd stays valid
        // even after we drop privileges.
        if (!platform_uinput_setup())
            throw std::runtime_error(
                "failed to initialize uinput device. Is the "
                "uinput kernel module loaded?");
        if (!platform_keyboard_capture_setup())
            throw std::runtime_error(
                "failed to initialize keyboard event device. Is "
                "the input kernel module loaded?");

        // Drop root so GUI runs under the original user's display session
        polkit_drop_privileges();
#endif
        // run uni init
        // errs were catched by `try`
        init_cfg_dir_properties();
        mkdirs(platform_cfg_dir);
        touch_config_file(platform_cfg_dir, config_name);
        refresh_config();

        // pre-parse every enabled hotkey's script into cached macro_scripts
        warmup_macros();

        // Background listener: polls each configured key combo and toggles its
        // macro on the rising edge (all keys of the combo become pressed).
        // Longer combos win over shorter ones that are strict subsets
        // (e.g. while Ctrl+L is held, plain L does not fire).
        // NOTE: use vector<char> instead of vector<bool> — vector<bool> is a
        // bit-packed specialization with proxy references and no address-of.
        g_listener = std::jthread([](std::stop_token st) {
            std::vector<char> prev(keys_properties.size(),
                                   0);  // combo fully-pressed last round
            while (!st.stop_requested()) {
                // evaluate the current pressed state of every enabled combo
                std::vector<char> now(keys_properties.size(), 0);
                for (std::size_t i = 0; i < keys_properties.size(); ++i) {
                    const auto& prop = keys_properties[i];
                    if (!prop.enabled || prop.keys.empty()) continue;
                    bool all = true;
                    for (auto k : prop.keys)
                        if (!keyboard::is_key_pressed(k)) {
                            all = false;
                            break;
                        }
                    now[i] = all ? 1 : 0;
                }

                // rising edges: fully pressed now, was not before
                std::vector<std::size_t> rising;
                for (std::size_t i = 0; i < keys_properties.size(); ++i)
                    if (now[i] && !prev[i]) rising.push_back(i);
                // longest combo first, so modifiers win over their subsets
                std::sort(rising.begin(), rising.end(),
                          [](std::size_t a, std::size_t b) {
                              return keys_properties[a].keys.size() >
                                     keys_properties[b].keys.size();
                          });

                // fire, suppressing any combo whose keys overlap a longer one
                std::vector<keyboard::keys> occupied;
                for (std::size_t idx : rising) {
                    const auto& combo = keys_properties[idx].keys;
                    bool blocked = false;
                    for (auto k : combo)
                        if (std::find(occupied.begin(), occupied.end(), k) !=
                            occupied.end()) {
                            blocked = true;
                            break;
                        }
                    if (blocked) continue;
                    macro::toggle(combo);
                    occupied.insert(occupied.end(), combo.begin(), combo.end());
                }

                prev = std::move(now);
                std::this_thread::sleep_for(std::chrono::milliseconds(4));
            }
        });
    } catch (const std::exception& e) {
        log_msg("moused: %s\n", e.what());
        return false;
    }

    // run ui
    mainWindow* mw = new mainWindow("moused");
    mw->Show(true);

    return true;
}

int moused::OnExit() {
    // stop the key listener thread
    if (g_listener.joinable()) g_listener.request_stop();
    // join it (jthread also auto-joins at destruction, but do it here so
    // the listener can't race our macro shutdown)
    if (g_listener.joinable()) g_listener.join();

    // stop & join every running macro worker
    macro::shutdown();

    return wxApp::OnExit();
}

// On Unix, elevate (via pkexec) BEFORE wxEntry: gtk_init_check() runs before
// wxApp::OnInit(), so the env restore done in the root branch must happen
// before GTK initializes, otherwise the pkexec-spawned root process dies with
// "Unable to initialize GTK+" (empty DISPLAY/WAYLAND environment).
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__)
wxIMPLEMENT_APP_NO_MAIN(moused);

int main(int argc, char* argv[]) {
    try {
        polkit_root_getter(argc, argv);
    } catch (const std::exception& e) {
        log_msg("moused: %s\n", e.what());
        return -1;
    }
    return wxEntry(argc, argv);
}
#else
// Macro that generates the standard main() entry point execution block
wxIMPLEMENT_APP(moused);
#endif
