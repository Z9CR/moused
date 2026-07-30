#include <adapter.hpp>
#include <ui.hpp>
#include <config.hpp>
#include <polkit_utils.hpp>
#include <toml.hpp>
extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#include "LuaBridge\LuaBridge.h"
// debug incs
#include <iostream>
using keys = keyboard::keys;

int main(int argc, char *argv[])
{
// run linux&bsds init
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    // Elevate to root (via pkexec if needed)
    polkit_root_getter(argc, argv);

    // Open /dev/uinput while we are still root — the fd stays valid
    // even after we drop privileges.
    if (!platform_uinput_setup())
    {
        fprintf(stderr, "moused: failed to initialize uinput device. "
                        "Is the uinput kernel module loaded?\n");
        return 1;
    }

    // init keyboard capture prog
    if (!platform_keyboard_capture_setup())
    {
        fprintf(stderr, "moused: failed to initialize keyboard event device. "
                        "Is the input kernel module loaded?\n");
        return 1;
    }

    // Drop root so GUI runs under the original user's display session
    polkit_drop_privileges();
#endif

    // run uni init
    if (!init_cfg_dir_properties())
        return 1;

    // debug
    std::cout << platform_cfg_dir;

    /*debug only
    while(1) {
        if (keyboard::is_key_pressed(keys::L))
        {
            mouse::translate(0, 10);
        }
        else if (keyboard::is_key_pressed(keys::J))
        {
            mouse::translate(90, 10);
        }
        else if (keyboard::is_key_pressed(keys::H))
        {
            mouse::translate(180, 10);
        }
        else if (keyboard::is_key_pressed(keys::K))
        {
            mouse::translate(-90, 10);
        }
        for (volatile int i = 0;i < 100000;i ++) {}
    }
    */
    return 0;
}
