#include <cstdio>
#undef NULL  // avoid conflict with keyboard::keys::NULL
#include <adapter.hpp>
#ifndef NULL
#define NULL 0  // restore NULL for standard compliance
#endif
#include <ui.hpp>
#include <config.hpp>
#include <polkit_utils.hpp>
// auto gen by slint
// if error, just ignore
#include <mainwindow.h>

using keys = keyboard::keys;

/*debug case
#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep(1000 * x)
#else
#include <unistd.h>
#endif
*/

int main(int argc, char *argv[])
{
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
    // `mw` stands for `mainwindow`
    auto mw = MainWindow::create();
    auto tray = Tray::create();
    auto window_weak = slint::ComponentWeakHandle(mw);
    mw->set_mainwindow_height(mainwindow_height);
    mw->set_mainwindow_width(mainwindow_width);
    mw->show();
    tray->show();
    slint::run_event_loop();
    //*/
    /*
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
        sleep(0.02);
    }
    */
    return 0;
}
