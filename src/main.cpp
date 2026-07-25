#include <definitions.hpp>
#include <adapter.hpp>
#include <ui.hpp>
// the lib below will not be compiled only on Linux&BSD
#include <polkit_utils.hpp>

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
    polkit_root_getter()
#endif
    
    return 0;
}