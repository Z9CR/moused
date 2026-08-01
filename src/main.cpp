#include <adapter.hpp>
#include <ui.hpp>
#include <config.hpp>
#include <utils.hpp>
#include <polkit_utils.hpp>
#include <toml.hpp>
extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#include "LuaBridge/LuaBridge.h"

bool moused::OnInit()
{
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    // Elevate to root (via pkexec if needed)
    polkit_root_getter(argc, argv);

    // Open /dev/uinput while we are still root — the fd stays valid
    // even after we drop privileges.
    if (!platform_uinput_setup())
    {
        log_msg("moused: failed to initialize uinput device. "
                "Is the uinput kernel module loaded?\n");
        return false;
    }

    // init keyboard capture prog
    if (!platform_keyboard_capture_setup())
    {
        log_msg("moused: failed to initialize keyboard event device. "
                "Is the input kernel module loaded?\n");
        return false;
    }

    // Drop root so GUI runs under the original user's display session
    polkit_drop_privileges();
#endif
    // run uni init
    if (!init_cfg_dir_properties())
    {
        log_msg("moused: error occured when fetching config path\n");
        return false;
    }
    if (!mkdirs(platform_cfg_dir)) {
        log_msg("moused: error occured when making config path\n");
        return false;
    }
    if (!touch_config_file(platform_cfg_dir, config_name)) {
        log_msg("moused: error occured when creating config file\n");
        return false;
    }

    // run ui
    mainWindow *mw = new mainWindow("moused", mainwindow_width, mainwindow_height);
    mw->Show(true);
    return true;
}

// Macro that generates the standard main() entry point execution block
wxIMPLEMENT_APP(moused);