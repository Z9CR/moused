#include <definitions.hpp>
#include <adapter.hpp>
#include <ui.hpp>
#include <config.hpp>
// the lib below will not be compiled only on Linux&BSD
#include <polkit_utils.hpp>
// auto gen by slint
// if error, just ignore
#include <mainwindow.h>

// debug tools
/*
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
    polkit_root_getter(argc, argv);
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
    return 0;
}