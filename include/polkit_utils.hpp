// use ifndef&define to pragma once
// Only Linux and FreeBSD elevate via polkit/pkexec: both need root to open
// /dev/uinput. OpenBSD/NetBSD use the wscons mux (no polkit/pkexec there)
// and are launched with doas/sudo directly, so they never compile this file.
#if defined(__linux__) || defined(__FreeBSD__)
#ifndef POLKIT_UTILS
#define POLKIT_UTILS

/// find absolute path of current executable
/// on Linux uses /proc/self/exe, on FreeBSD uses realpath(argv[0])
/// unix & unix-likes only!
// only used in inner .cpp
// static const char *get_exe_path(const char *argv0);

// the program must have privilege in Linux and FreeBSD to RW /dev/uinput
void polkit_root_getter(int argc, char* argv[]);

/// Drop root privileges back to the original user.
/// Call this after platform_uinput_setup() has succeeded.
void polkit_drop_privileges();

#endif
#endif
