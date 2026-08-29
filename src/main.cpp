#pragma region includes
#include <wx/filefn.h>    // wxPathOnly
#include <wx/intl.h>      // wxLocale / wxFileTranslationsLoader
#include <wx/stdpaths.h>  // wxStandardPaths
#include <adapter.hpp>
#include <algorithm>
#include <chrono>
#include <config.hpp>
#include <macro.hpp>
#include <macro_manager.hpp>
#include <polkit_utils.hpp>
#include <stdexcept>
#include <thread>
#include <toml.hpp>
#include <ui.hpp>
#include <utils.hpp>
#pragma endregion

namespace {
// Background listener: polls the keyboard and toggles macros on the
// rising edge of a registered hotkey press.
std::jthread g_listener;
}  // namespace

bool moused::OnInit() {
    // i18n: load `locale/<lang>/LC_MESSAGES/moused.mo` placed next to the
    // executable (the build deploys it there; see CMakeLists.txt). `_()` falls
    // back to the original string when a catalog/translation is missing, so a
    // missing .mo is never fatal.
    m_locale = std::make_unique<wxLocale>(wxLANGUAGE_DEFAULT,
                                          wxLOCALE_DONT_LOAD_DEFAULT);
    wxFileTranslationsLoader::AddCatalogLookupPathPrefix(
        wxPathOnly(wxStandardPaths::Get().GetExecutablePath()) +
        wxFILE_SEP_PATH + "locale");
    m_locale->AddCatalog("moused");

    try {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__)
        // Linux & FreeBSD elevate in main() BEFORE wxEntry/GTK init via
        // pkexec (the re-exec'd root process needs the display env restored
        // from the envfile before gtk_init_check()). OpenBSD/NetBSD/DragonFly
        // have no reliable polkit/pkexec, so there the user launches the app
        // with doas/sudo directly. On Linux & FreeBSD /dev/uinput is opened
        // while still root — the fd stays valid even after we drop
        // privileges.
        if (!platform_uinput_setup())
            throw std::runtime_error(
                "failed to initialize the input device. On Linux/FreeBSD "
                "is the uinput kernel module loaded? On OpenBSD/NetBSD is "
                "the wscons mouse mux available, on DragonFly an evdev REL "
                "mouse device (run with doas/sudo)?");
#if defined(__linux__)
        // Linux: evdev is guaranteed on a stock desktop kernel, so a failed
        // keyboard capture is fatal.
        if (!platform_keyboard_capture_setup())
            throw std::runtime_error(
                "failed to initialize keyboard event device. Is "
                "the input kernel module loaded?");
#else
        // FreeBSD: same evdev EVIOCGKEY capture as Linux, but best-effort
        // like OpenBSD/NetBSD/DragonFly — a missing /dev/input/event*
        // keyboard node (e.g. a PS/2-only setup) must not keep the GUI
        // from starting.
        if (!platform_keyboard_capture_setup())
            log_msg("moused: continuing without hotkey capture\n");
#endif
        // Drop root so GUI runs under the original user's display session
        polkit_drop_privileges();
#else
        // OpenBSD/NetBSD: wscons exposes no key-state query to a second
        // process while the window system owns the keyboard; DragonFly can
        // have an evdev-less kernel. Keyboard capture is best-effort on all
        // three — a failure is logged, not fatal.
        if (!platform_keyboard_capture_setup())
            log_msg("moused: continuing without hotkey capture\n");
#endif
#endif
        // run uni init
        // errs were catched by `try`
        init_cfg_dir_properties();
        mkdirs(platform_cfg_dir);
        touch_config_file(platform_cfg_dir, config_name);
        read_from_config();

        // i18n: apply the language persisted in [global].language (defaults
        // to following the system UI language).
        applyConfigLanguage();

        // register every enabled macro into the runtime macro manager
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
    // `silent_launch = true` starts the app into the tray only; fall back to
    // showing the window when the tray icon could not be created, otherwise
    // the app would be invisible with no way to quit.
    if (!silent_launch || !mw->hasTray()) mw->Show(true);
    return true;
}

int moused::OnExit() {
    auto t0 = std::chrono::steady_clock::now();

    // CRITICAL: set the global shutdown flag FIRST, before any join.
    // Long-running mouse loops (move_to/translate smooth moves on Windows
    // & Linux) poll this flag every frame and bail out immediately, so the
    // unbounded joins below can never be blocked by an in-flight macro.
    macro::request_global_shutdown();

    // stop the key listener thread
    if (g_listener.joinable()) g_listener.request_stop();
    // join it (jthread also auto-joins at destruction, but do it here so
    // the listener can't race our macro shutdown)
    if (g_listener.joinable()) g_listener.join();
    log_msg("moused: listener joined in %d ms\n",
            static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - t0)
                                 .count()));

    // stop & join every running macro worker
    macro::shutdown();
    log_msg("moused: macro shutdown in %d ms\n",
            static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - t0)
                                 .count()));

    return wxApp::OnExit();
}

// On Linux & FreeBSD, elevate (via pkexec) BEFORE wxEntry: gtk_init_check()
// runs before wxApp::OnInit(), so the env restore done in the root branch
// must happen before GTK initializes, otherwise the pkexec-spawned root
// process dies with "Unable to initialize GTK+" (empty DISPLAY/WAYLAND
// environment). OpenBSD/NetBSD/DragonFly skip this: the user launches with
// doas/sudo directly (no reliable polkit/pkexec there).
#if defined(__linux__) || defined(__FreeBSD__)
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
