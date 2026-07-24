#include <cstdio>
#include <definitions.hpp>
#include <adapter.hpp>
// debug tools
/*
#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep(1000 * x)
#else
#include <unistd.h>
#endif
*/

/// find absolute path of current executable
/// on Linux uses /proc/self/exe, on BSDs uses realpath(argv[0])
/// unix & unix_Likes only!
static const char* get_exe_path(const char* argv0) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>

    // PATH_MAX is in limits.h when posix
static char path[PATH_MAX];

#if defined(__linux__)
    // Linux: /proc/self/exe is always reliable
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        return path;
    }
#endif
    // BSDs / fallback: resolve argv[0]
    // realpath() is in stdlib.h when posix
    if (realpath(argv0, path))
        return path;

    return argv0; // last resort
#endif
}

int main(int argc, char* argv[]) {
    // the program must have privilege in Linux and BSDs to RW /dev/uinput(Linux) or /dev/wsmouse(BSDs)
    #if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    if (geteuid() != 0) {
        // try polkit self-elevation via pkexec
        const char* me = get_exe_path(argc > 0 ? argv[0] : "moused");
        execlp("pkexec", "pkexec", me, (char *)NULL);
        
        // pkexec failed (not installed / user cancelled / no desktop session)
        // fallback: suggest manual sudo/doas
        fprintf(stderr, "moused: root required (need write access to /dev/uinput).\n");
        fprintf(stderr, "  polkit elevation failed — run with: sudo moused\n");
        return 1;
    }
    #endif

    return 0;
}