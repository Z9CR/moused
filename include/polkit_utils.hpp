// use ifndef&define to pragma once
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#ifndef POLKIT_UTILS
#define POLKIT_UTILS

/// find absolute path of current executable
/// on Linux uses /proc/self/exe, on BSDs uses realpath(argv[0])
/// unix & unix-likes only!
// only used in inner .cpp
// static const char *get_exe_path(const char *argv0);

// the program must have privilege in Linux and BSDs to RW /dev/uinput(Linux) or /dev/wsmouse(BSDs)
void polkit_root_getter(int argc, char* argv[]);

#endif
#endif