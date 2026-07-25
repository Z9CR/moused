#include <polkit_utils.hpp>
// avoid errors on win
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
/// find absolute path of current executable
/// on Linux uses /proc/self/exe, on BSDs uses realpath(argv[0])
/// unix & unix-likes only!
static const char *get_exe_path(const char *argv0)
{
    static char path[PATH_MAX];

#if defined(__linux__)
    // Linux: /proc/self/exe is always reliable
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1)
    {
        path[len] = '\0';
        return path;
    }
#endif
    // BSDs / fallback: resolve argv[0]
    if (realpath(argv0, path))
        return path;

    return argv0; // last resort
}

// the program must have privilege in Linux and BSDs to RW /dev/uinput(Linux) or /dev/wsmouse(BSDs)
void polkit_root_getter()
{
    if (geteuid() != 0)
    {
        // try polkit self-elevation via pkexec
        const char *me = get_exe_path(argc > 0 ? argv[0] : "moused");
        execlp("pkexec", "pkexec", me, (char *)NULL);

        // pkexec failed (not installed / user cancelled / no desktop session)
        // fallback: suggest manual sudo/doas
        fprintf(stderr, "moused: root required (need write access to /dev/uinput).\n");
        fprintf(stderr, "  polkit elevation failed — run with: sudo moused\n");
        return 1;
    }
    (void)argc;
    (void)argv;
}
#endif