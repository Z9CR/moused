#include <adapter.hpp>
#include <config.hpp>
#include <utils.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <toml.hpp>
#include <vector>
#include <string>
#include <format>

// fallbacks
int mainwindow_width = 300;

int mainwindow_height = 500;

constexpr int smoothmv_frametime = 4;

const std::string mainwindow_title = "moused";

// global config directory path, static lifetime
std::string platform_cfg_dir;

// cfg name
const std::string config_name = "config.toml";

// get `std::string platform_cfg_dir`
#pragma region init_cfg_dir_properties()
#if defined(_WIN32)
// windows
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

// Fetch the Roaming AppData path, write into platform_cfg_dir
// run it when init
void init_cfg_dir_properties()
{
    char buf[MAX_PATH]{};

    // Primary: SHGetFolderPathA — ANSI, no conversion, no CoTaskMemFree
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, buf) == S_OK)
    {
        platform_cfg_dir = buf;
        platform_cfg_dir += "\\moused\\";
        return;
    }

    // Fallback 1: use %APPDATA% env var
    if (GetEnvironmentVariableA("APPDATA", buf, static_cast<DWORD>(sizeof(buf))) > 0)
    {
        platform_cfg_dir = buf;
        platform_cfg_dir += "\\moused\\";
        return;
    }

    // Fallback 2: use current directory
    GetCurrentDirectoryA(static_cast<DWORD>(sizeof(buf)), buf);
    platform_cfg_dir = buf;
    platform_cfg_dir += "\\moused";
}

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
// linux&&bsds

#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

// Fetch the `~/.config/moused/`, write into platform_cfg_dir
// Uses getpwuid to reliably get the user's home directory
// even when running via pkexec/sudo (where HOME was set to /root)
void init_cfg_dir_properties()
{
    const char *home = nullptr;
    // When launched via pkexec/sudo, SUDO_UID / PKEXEC_UID tell us who
    // the real user is. Use getpwuid to reliably resolve their home dir
    // regardless of what $HOME currently says (privileges may already
    // be dropped, so do NOT gate this on geteuid()==0).
    {
        uid_t orig_uid = (uid_t)-1;
        const char *sudo_uid = std::getenv("SUDO_UID");
        const char *pkexec_uid = std::getenv("PKEXEC_UID");
        if (sudo_uid)
            orig_uid = (uid_t)std::atoi(sudo_uid);
        else if (pkexec_uid)
            orig_uid = (uid_t)std::atoi(pkexec_uid);

        if (orig_uid != (uid_t)-1 && orig_uid != 0)
        {
            struct passwd *pw = getpwuid(orig_uid);
            if (pw && pw->pw_dir)
                home = pw->pw_dir;
        }
    }

    // Fallback: use HOME environment variable (works when running as
    // normal user without pkexec/sudo, or if getpwuid failed)
    if (!home)
    {
        home = std::getenv("HOME");
    }

    if (home)
    {
        std::filesystem::path cfg{home};
        cfg /= ".config";
        cfg /= "moused";
        platform_cfg_dir = cfg.string();
        return;
    }
    throw std::runtime_error("failed to fetch config path");
}
#elif defined(__APPLE__)
// MacOS
#else
// unk sys
#endif
#pragma endregion

enum class script_type
{
    in_line = 0,
    file = 1
};

struct loopment
{
    bool enabled;
    // when `-1`, it will be casted into `18446744073709551615`
    unsigned long long times;
    // unit: ms
    double delay;
};

struct key_property
{
    keyboard::keys key;
    bool enabled;
    script_type type;
    std::string code;
    loopment loop;
};

// this var to store properties
std::vector<key_property> keys_properties{};

void touch_config_file(const std::string &parent_path, const std::string &name)
{
    // if the file already exists, do nothing
    std::filesystem::path cfg(parent_path);
    cfg /= name;
    if (std::filesystem::exists(cfg))
    {
        return;
    }

    std::ofstream out(cfg);
    if (!out)
    {
        throw std::runtime_error("failed to create config file");
    }
}

void refresh_config()
{
    std::filesystem::path cfg(platform_cfg_dir);
    cfg /= config_name;
    toml::value _props = toml::parse(cfg.string());
    if (_props.is_empty())
    {
        keys_properties.clear();
        return;
    }
    const auto &props = _props.as_table();
    for (const auto &[key, val] : props)
    {
        // fetch global
        if (key == "global")
        {
        }
        // every profile of a key should be a table
        if (!val.is_table())
        {
            throw std::runtime_error(std::format("moused: error occured when parsing config file `{}`, \nerror key is `{}`", cfg.string(), key));
            return;
        }
        // sub table of the key
        const auto &key_profile = val.as_table();
        // 2 structures to save profile temporally
        loopment _loopment{};
        key_property _property{};
        // gen keyboard::keys->int map
        const struct
        {
            std::string_view k;
            int val;
        } table[] = {
#define KEYS_ITEM(name, value) {#name, value},
            KEYS_LIST(KEYS_ITEM)
#undef KEYS_ITEM
        }

        /*
        [key]
        enabled = <bool>
        type = 'inline' or 'file'
        val = """lua code(if type='inline')"
        [key.loop]
        enabled = <bool>
        times = <int>
        delay = <double>
        */
    }
}