#include <config.hpp>

const int mainwindow_width = 300;

const int mainwindow_height = 500;

// global config directory buffer, static lifetime
char platform_cfg_dir[512];

// get `const char* platform_cfg_dir`
#if defined(_WIN32)
// windows
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

// Fetch the Roaming AppData path, write into platform_cfg_dir
// run it when init
bool init_cfg_dir_properties()
{   
    // Primary: SHGetFolderPathA — ANSI, no conversion, no CoTaskMemFree
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, platform_cfg_dir) == S_OK)
        return true;

    // Fallback 1: use %APPDATA% env var
    if (GetEnvironmentVariableA("APPDATA", platform_cfg_dir, sizeof(platform_cfg_dir)) > 0)
        return true;

    // Fallback 2: use current directory
    GetCurrentDirectoryA(sizeof(platform_cfg_dir), platform_cfg_dir);
    return true;
}

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
// linux&&bsds
#elif defined(__APPLE__)
// MacOS
#else
// unk sys
#endif

const char* cfg_path;

const int smoothmv_frametime = 4;
