#include <adapter.hpp>
#include <ui.hpp>
#include <config.hpp>
#include <utils.hpp>
#include <macro.hpp>
#include <macro_manager.hpp>
#include <script_binding.hpp>
#include <polkit_utils.hpp>
#include <toml.hpp>
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>
extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#include "LuaBridge/LuaBridge.h"

namespace
{
    // Pre-parse every enabled hotkey's Lua script into a cached macro_script.
    // Scripts are executed ONCE at startup; at runtime only the cached
    // command arrays run (no Lua).
    void warmup_macros()
    {
        lua_State *L = luaL_newstate();
        if (!L)
        {
            log_msg("moused: failed to create lua state\n");
            return;
        }
        luaL_openlibs(L);
        register_script_enums(L);

        for (const auto &prop : keys_properties)
        {
            if (!prop.enabled)
                continue;

            std::string script;
            if (prop.type == script_type::in_line)
            {
                script = prop.code;
            }
            else if (prop.type == script_type::file)
            {
                std::ifstream f(prop.code);
                if (!f)
                {
                    log_msg("moused: cannot open script file `%s`\n", prop.code.c_str());
                    continue;
                }
                script.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
            }
            else
            {
                log_msg("moused: unknown script type for key %d\n", static_cast<int>(prop.key));
                continue;
            }

            if (luaL_loadbuffer(L, script.data(), script.size(), "hotkey") != LUA_OK ||
                lua_pcall(L, 0, 0, 0) != LUA_OK)
            {
                log_msg("moused: error loading script for key %d: %s\n",
                        static_cast<int>(prop.key), lua_tostring(L, -1));
                lua_pop(L, 1);
                continue;
            }

            lua_getglobal(L, "run");
            if (!lua_isfunction(L, -1))
            {
                log_msg("moused: script for key %d has no `run()`\n", static_cast<int>(prop.key));
                lua_pop(L, 1);
                continue;
            }
            if (lua_pcall(L, 0, 1, 0) != LUA_OK)
            {
                log_msg("moused: run() failed for key %d: %s\n",
                        static_cast<int>(prop.key), lua_tostring(L, -1));
                lua_pop(L, 1);
                continue;
            }

            try
            {
                macro_script ms = parse_lua_result(L);
                macro::register_macro(prop.key, ms, prop.loop);
            }
            catch (const std::exception &e)
            {
                log_msg("moused: parse failed for key %d: %s\n",
                        static_cast<int>(prop.key), e.what());
            }
            lua_settop(L, 0);
        }

        lua_close(L);
    }

    // Background listener: polls the keyboard and toggles macros on the
    // rising edge of a registered hotkey press.
    std::jthread g_listener;
} // namespace

// debug only: load every hotkey's Lua script into a fresh state and parse it
/* into a macro_script, printing each parsed command.
#pragma region test_parse_lua_config
static void test_parse_lua_config()
{
    lua_State *L = luaL_newstate();
    if (!L)
    {
        log_msg("moused: test: failed to create lua state\n");
        return;
    }
    luaL_openlibs(L);
    register_script_enums(L);

    // debug: dump the enum tables actually visible to the scripts
    lua_getglobal(L, "mouse_btn");
    log_msg("debug: mouse_btn type=%s", lua_typename(L, lua_type(L, -1)));
    if (lua_istable(L, -1))
    {
        lua_getfield(L, -1, "LMB");
        log_msg(", LMB type=%s", lua_typename(L, lua_type(L, -1)));
        if (lua_isnumber(L, -1))
            log_msg(" value=%d", static_cast<int>(lua_tointeger(L, -1)));
    }
    log_msg("\n");
    lua_pop(L, 2);
    lua_getglobal(L, "wheel_rotation");
    log_msg("debug: wheel_rotation type=%s", lua_typename(L, lua_type(L, -1)));
    if (lua_istable(L, -1))
    {
        lua_getfield(L, -1, "WU");
        log_msg(", WU type=%s", lua_typename(L, lua_type(L, -1)));
        if (lua_isnumber(L, -1))
            log_msg(" value=%d", static_cast<int>(lua_tointeger(L, -1)));
    }
    log_msg("\n");
    lua_pop(L, 2);

    // cmd enum value -> name lookup, for pretty-printing only
    static const struct
    {
        int v;
        const char *name;
    } cmd_names[] = {
#define COMMAND_NAME_ITEM(name, value) {value, #name},
        COMMAND_LIST(COMMAND_NAME_ITEM)
#undef COMMAND_NAME_ITEM
    };

    for (const auto &prop : keys_properties)
    {
        // load the script text: inline code, or read the file
        std::string script;
        if (prop.type == script_type::in_line)
        {
            script = prop.code;
        }
        else if (prop.type == script_type::file)
        {
            std::ifstream f(prop.code);
            if (!f)
            {
                log_msg("moused: test: cannot open script file `%s`\n", prop.code.c_str());
                continue;
            }
            script.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        }
        else
        {
            log_msg("moused: test: unknown script type for key %d\n", static_cast<int>(prop.key));
            continue;
        }

        // compile + run the chunk, which defines the global `run()`
        if (luaL_loadbuffer(L, script.data(), script.size(), "hotkey") != LUA_OK ||
            lua_pcall(L, 0, 0, 0) != LUA_OK)
        {
            log_msg("moused: test: error loading script for key %d: %s\n",
                    static_cast<int>(prop.key), lua_tostring(L, -1));
            lua_pop(L, 1);
            continue;
        }

        // call run(), expecting 1 return value
        lua_getglobal(L, "run");
        if (!lua_isfunction(L, -1))
        {
            log_msg("moused: test: script for key %d has no `run()`\n", static_cast<int>(prop.key));
            lua_pop(L, 1);
            continue;
        }
        if (lua_pcall(L, 0, 1, 0) != LUA_OK)
        {
            log_msg("moused: test: run() failed for key %d: %s\n",
                    static_cast<int>(prop.key), lua_tostring(L, -1));
            lua_pop(L, 1);
            continue;
        }

        try
        {
            macro_script ms = parse_lua_result(L);
            log_msg("---- key %d: %zu commands ----\n", static_cast<int>(prop.key), ms.size());
            for (const auto &c : ms)
            {
                const char *cname = "?";
                for (const auto &nm : cmd_names)
                    if (nm.v == static_cast<int>(c.type))
                    {
                        cname = nm.name;
                        break;
                    }
                log_msg("  cmd=%-10s delay=%.1f  args=[", cname, c.delay);
                for (std::size_t i = 0; i < c.args.size(); ++i)
                    log_msg(i == 0 ? "%g" : ", %g", c.args[i]);
                log_msg("]\n");
            }
        }
        catch (const std::exception &e)
        {
            log_msg("moused: test: parse failed for key %d: %s\n",
                    static_cast<int>(prop.key), e.what());
        }

        lua_settop(L, 0); // drop the return table + any leftovers
    }

    lua_close(L);
}
#pragma endregion
*/

bool moused::OnInit()
{
    try
    {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
        // Elevate to root (via pkexec if needed)
        polkit_root_getter(argc, argv);

        // Open /dev/uinput while we are still root — the fd stays valid
        // even after we drop privileges.
        if (!platform_uinput_setup())
            throw std::runtime_error("failed to initialize uinput device. Is the uinput kernel module loaded?");
        if (!platform_keyboard_capture_setup())
            throw std::runtime_error("failed to initialize keyboard event device. Is the input kernel module loaded?");

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

        // start the background key listener; it toggles macros on the
        // rising edge of a hotkey press and never blocks the GUI
        g_listener = std::jthread([](std::stop_token st)
                                  {
            keyboard::keys prev = keyboard::keys::NONE;
            while (!st.stop_requested())
            {
                keyboard::keys cur = keyboard::get_key_pressed();
                // rising edge: a key was NOT pressed and now IS
                if (cur != prev && cur != keyboard::keys::NONE)
                    macro::toggle(cur);
                prev = cur;
                std::this_thread::sleep_for(std::chrono::milliseconds(4));
            } });
    }
    catch (const std::exception &e)
    {
        log_msg("moused: %s\n", e.what());
        return false;
    }

    /*debug
    #pragma region debug
    for (const auto &prop : keys_properties)
    {
        log_msg("common\n");
        log_msg("enabled: %d\n", prop.enabled);
        log_msg("key: %d\n", static_cast<int>(prop.key));
        log_msg("script_type: %d\n", static_cast<int>(prop.type));
        log_msg("code: %s\n", prop.code.c_str());
        log_msg("loop\n");
        log_msg("enabled: %d\n", prop.loop.enabled);
        log_msg("delay: %f\n", prop.loop.delay);
        log_msg("times: %llu\n", prop.loop.times);
        log_msg("\n");
    }
    // debug only: read every hotkey's Lua script and parse it into commands
    test_parse_lua_config();
    #pragma endregion
    */
    // run ui
    mainWindow *mw = new mainWindow("moused", mainwindow_width, mainwindow_height);
    mw->Show(true);

    return true;
}

int moused::OnExit()
{
    // stop the key listener thread
    if (g_listener.joinable())
        g_listener.request_stop();
    // join it (jthread also auto-joins at destruction, but do it here so
    // the listener can't race our macro shutdown)
    if (g_listener.joinable())
        g_listener.join();

    // stop & join every running macro worker
    macro::shutdown();

    return wxApp::OnExit();
}

// Macro that generates the standard main() entry point execution block
wxIMPLEMENT_APP(moused);
