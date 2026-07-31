#include <config.hpp>

const int mainwindow_width = 300;

const int mainwindow_height = 500;

const char* mainwindow_title = "moused";

// global config directory buffer, static lifetime
// new but not delete to promise the static lifetime
char* platform_cfg_dir = new char[1024];

// get `const char* platform_cfg_dir`
#if defined(_WIN32)
// windows
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <cstring>
// Fetch the Roaming AppData path, write into platform_cfg_dir
// run it when init
bool init_cfg_dir_properties()
{
    // Primary: SHGetFolderPathA — ANSI, no conversion, no CoTaskMemFree
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, platform_cfg_dir) == S_OK)
    {
        strcat(platform_cfg_dir, "\\moused\\");
        return true;
    }

    // Fallback 1: use %APPDATA% env var
    else if (GetEnvironmentVariableA("APPDATA", platform_cfg_dir, sizeof(platform_cfg_dir)) > 0)
    {
        strcat(platform_cfg_dir, "\\moused\\");
        return true;
    }

    // Fallback 2: use current directory
    else
    {
        GetCurrentDirectoryA(sizeof(platform_cfg_dir), platform_cfg_dir);
        strcat(platform_cfg_dir, "\\moused");
        return true;
    }
    return false;
}

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
// linux&&bsds

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

// Fetch the `~/.config/moused/`, write into platform_cfg_dir
// Uses getpwuid to reliably get the user's home directory
// even when running via pkexec/sudo (where HOME was set to /root)
bool init_cfg_dir_properties()
{
    const char* home = nullptr;

    // When launched via pkexec/sudo, SUDO_UID / PKEXEC_UID tell us who
    // the real user is. Use getpwuid to reliably resolve their home dir
    // regardless of what $HOME currently says (privileges may already
    // be dropped, so do NOT gate this on geteuid()==0).
    {
        uid_t orig_uid = (uid_t)-1;
        const char* sudo_uid = std::getenv("SUDO_UID");
        const char* pkexec_uid = std::getenv("PKEXEC_UID");
        if (sudo_uid)
            orig_uid = (uid_t)std::atoi(sudo_uid);
        else if (pkexec_uid)
            orig_uid = (uid_t)std::atoi(pkexec_uid);

        if (orig_uid != (uid_t)-1 && orig_uid != 0) {
            struct passwd* pw = getpwuid(orig_uid);
            if (pw && pw->pw_dir)
                home = pw->pw_dir;
        }
    }

    // Fallback: use HOME environment variable (works when running as
    // normal user without pkexec/sudo, or if getpwuid failed)
    if (!home) {
        home = std::getenv("HOME");
    }

    if (home) {
        strncpy(platform_cfg_dir, home, 1023);
        platform_cfg_dir[1023] = '\0';
        strncat(platform_cfg_dir, "/.config/moused/", 1023 - strlen(platform_cfg_dir));
        return true;
    }
    return false;
}
#elif defined(__APPLE__)
// MacOS
#else
// unk sys
#endif

const char *cfg_path;

const int smoothmv_frametime = 4;