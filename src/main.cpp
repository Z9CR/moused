#include <adapter.hpp>
#include <ui.hpp>
#include <config.hpp>
// the lib below will not be compiled only on Linux&BSD
#include <polkit_utils.hpp>
// auto gen by slint
// if error, just ignore
#include <mainwindow.h>

using keys = keyboard::keys;
// debug tools

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep(1000 * x)
#else
#include <unistd.h>
#endif


int main(int argc, char *argv[])
{
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    polkit_root_getter(argc, argv);
#endif
    // `mw` stands for `mainwindow`
    ///* debug switch
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