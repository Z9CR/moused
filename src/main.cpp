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
        init_cfg_dir_properties();
        mkdirs(platform_cfg_dir);
        touch_config_file(platform_cfg_dir, config_name);
    }
    catch (const std::exception& e)
    {
        log_msg("moused: %s\n", e.what());
        return false;
    }

    // run ui
    mainWindow *mw = new mainWindow("moused", mainwindow_width, mainwindow_height);
    mw->Show(true);
    return true;
}

// Macro that generates the standard main() entry point execution block
wxIMPLEMENT_APP(moused);