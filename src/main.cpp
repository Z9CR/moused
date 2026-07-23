#include <cstdio>
#include <definitions.hpp>
#include <adapter.hpp>

#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <unistd.h>
#endif

// debug tools
/*
#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep(1000 * x)
#else
#include <unistd.h>
#endif
*/

int main() {
    // the program must have privilege in Linux and BSDs to RW /dev/uinput(Linux) or /dev/wsmouse(BSDs)
    #if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    if (geteuid() != 0) {
        fprintf(stderr, "moused: root required (need write access to /dev/uinput). "
                        "Run with: sudo moused\n");
        return 1;
    }
    #endif
    
    return 0;
}