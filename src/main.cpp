#include <adapter.hpp>
#include <ui.hpp>
#include <config.hpp>
#include <utils.hpp>
#include <polkit_utils.hpp>
#include <toml.hpp>
#include <stdexcept>
extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#include "LuaBridge/LuaBridge.h"

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
    }
    catch (const std::exception &e)
    {
        log_msg("moused: %s\n", e.what());
        return false;
    }

    //debug
    /*
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
    */
    // run ui
    mainWindow *mw = new mainWindow("moused", mainwindow_width, mainwindow_height);
    mw->Show(true);
    return true;
}

// Macro that generates the standard main() entry point execution block
wxIMPLEMENT_APP(moused);