#include <config.hpp>
#include <utils.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>

int mainwindow_width = 300;

int mainwindow_height = 500;

const char* mainwindow_title = "moused";

// global config directory buffer, fixed size, static lifetime
char platform_cfg_dir[1024];

// cfg name
const char* config_name = "config.toml";

// get `const char* platform_cfg_dir`
#if defined(_WIN32)
// windows
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <cstring>

// Fetch the Roaming AppData path, write into platform_cfg_dir
// run it when init
void init_cfg_dir_properties()
{
    // Primary: SHGetFolderPathA — ANSI, no conversion, no CoTaskMemFree
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, platform_cfg_dir) == S_OK)
    {
        std::strncat(platform_cfg_dir, "\\moused\\", sizeof(platform_cfg_dir) - std::strlen(platform_cfg_dir) - 1);
        return;
    }

    // Fallback 1: use %APPDATA% env var
    if (GetEnvironmentVariableA("APPDATA", platform_cfg_dir, sizeof(platform_cfg_dir)) > 0)
    {
        std::strncat(platform_cfg_dir, "\\moused\\", sizeof(platform_cfg_dir) - std::strlen(platform_cfg_dir) - 1);
        return;
    }

    // Fallback 2: use current directory
    GetCurrentDirectoryA(sizeof(platform_cfg_dir), platform_cfg_dir);
    std::strncat(platform_cfg_dir, "\\moused", sizeof(platform_cfg_dir) - std::strlen(platform_cfg_dir) - 1);
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
void init_cfg_dir_properties()
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
        std::filesystem::path cfg{home};
        cfg /= ".config";
        cfg /= "moused";
        std::strncpy(platform_cfg_dir, cfg.c_str(), sizeof(platform_cfg_dir) - 1);
        platform_cfg_dir[sizeof(platform_cfg_dir) - 1] = '\0';
        return;
    }
    throw std::runtime_error("failed to fetch config path");
}
#elif defined(__APPLE__)
// MacOS
#else
// unk sys
#endif


void touch_config_file(const char* parent_path, const char* name) {
    // if the file already exists, do nothing
    std::filesystem::path cfg(parent_path);
    cfg /= name;
    if (std::filesystem::exists(cfg)) {
        return;
    }

    std::ofstream out(cfg);
    if (!out) {
        throw std::runtime_error("failed to create config file");
    }
}

constexpr int smoothmv_frametime = 4;