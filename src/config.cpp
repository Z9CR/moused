#include <adapter.hpp>
#include <config.hpp>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <toml.hpp>
#include <utils.hpp>
#include <vector>

// fallbacks
int smoothmv_frametime = 4;

const std::string mainwindow_title = "moused";

// window size upper bound, falls back to 0 = "no max size"
int mainwindow_max_width = 0;
int mainwindow_max_height = 0;

// global config directory path, static lifetime
std::string platform_cfg_dir;

// cfg name
const std::string config_name = "config.toml";

// get `std::string platform_cfg_dir`
#pragma region init_cfg_dir_properties()
#if defined(_WIN32)
// windows
#include <shlobj.h>
#include <stdio.h>
#include <windows.h>

// Fetch the Roaming AppData path, write into platform_cfg_dir
// run it when init
void init_cfg_dir_properties() {
    char buf[MAX_PATH]{};

    // Primary: SHGetFolderPathA — ANSI, no conversion, no CoTaskMemFree
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, buf) == S_OK) {
        platform_cfg_dir = buf;
        platform_cfg_dir += "\\moused\\";
        return;
    }

    // Fallback 1: use %APPDATA% env var
    if (GetEnvironmentVariableA("APPDATA", buf,
                                static_cast<DWORD>(sizeof(buf))) > 0) {
        platform_cfg_dir = buf;
        platform_cfg_dir += "\\moused\\";
        return;
    }

    // Fallback 2: use current directory
    GetCurrentDirectoryA(static_cast<DWORD>(sizeof(buf)), buf);
    platform_cfg_dir = buf;
    platform_cfg_dir += "\\moused";
}

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__)
// linux&&bsds

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <string>

// Fetch the `~/.config/moused/`, write into platform_cfg_dir
// Uses getpwuid to reliably get the user's home directory
// even when running via pkexec/sudo (where HOME was set to /root)
void init_cfg_dir_properties() {
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
            if (pw && pw->pw_dir) home = pw->pw_dir;
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

// this var to store properties
std::vector<key_property> keys_properties{};

void touch_config_file(const std::string& parent_path,
                       const std::string& name) {
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

void refresh_config() {
    std::filesystem::path cfg(platform_cfg_dir);
    cfg /= config_name;
    toml::value _props = toml::parse(cfg.string());
    // reset to defaults before re-parsing, so repeated calls don't accumulate
    // fallbacks
    smoothmv_frametime = 4;
    keys_properties.clear();
    if (_props.is_empty()) return;
    const auto& props = _props.as_table();
    // gen keyboard::keys->string map
    const struct {
        int k;
        std::string v;
    } table[] = {
#define KEYS_ITEM(name, value) {value, #name},
        KEYS_LIST(KEYS_ITEM)
    // {0, "NULL"},
#undef KEYS_ITEM
    };
    for (const auto& [key, val] : props) {
        // fetch global
        if (key == "global") {
            const auto& global = val.as_table();
            smoothmv_frametime =
                static_cast<int>(global.at("smooth_frametime_ms").as_integer());
            // `val` is the [global] basic_value; find_or needs a basic_value
            // (or pointer), not a bare table, hence we search within val.
            mainwindow_max_width =
                static_cast<int>(toml::find_or(val, "max_window_width", 0));
            mainwindow_max_height =
                static_cast<int>(toml::find_or(val, "max_window_height", 0));
            continue;
        }
        // every profile of a key should be a table
        if (!val.is_table()) {
            throw std::runtime_error(std::format(
                "moused: error occurred when parsing config file `{}`",
                cfg.string()));
        }
        // sub table of the key
        const auto& key_profile = val.as_table();
        // 2 structures to save profile temporally
        loopment _loopment{};
        key_property _property{};
        // `keys` is MANDATORY: the combo is declared strictly in this array
        // (["LEFT_CONTROL", "L"]), never via the section name. Names come from
        // KEYS_LIST; unknown names are rejected here.
        auto name_to_key = [&](const std::string& n) -> keyboard::keys {
            for (const auto& item : table)
                if (item.v == n) return static_cast<keyboard::keys>(item.k);
            return keyboard::keys::NONE;
        };

        auto keys_it = key_profile.find("keys");
        if (keys_it == key_profile.end() || !keys_it->second.is_array()) {
            throw std::runtime_error(std::format(
                "moused: section `{}` is missing the required `keys = "
                "[...]` array in config file `{}`",
                key, cfg.string()));
        }
        const auto& arr = keys_it->second.as_array();
        for (const auto& v : arr) {
            const std::string n = v.as_string();
            keyboard::keys k = name_to_key(n);
            if (k == keyboard::keys::NONE)
                throw std::runtime_error(
                    std::format("moused: unknown key `{}` in config file `{}`",
                                n, cfg.string()));
            _property.keys.push_back(k);
        }
        if (_property.keys.empty())
            throw std::runtime_error(std::format(
                "moused: empty keys array in config file `{}`", cfg.string()));
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
        _property.enabled = key_profile.at("enabled").as_boolean();
        if (key_profile.at("type").as_string() == "file")
            _property.type = script_type::file;
        else
            _property.type = script_type::in_line;
        _property.code = key_profile.at("val").as_string();

        // [key.loop] sub table
        const auto& loop = key_profile.at("loop").as_table();
        _loopment.enabled = loop.at("enabled").as_boolean();
        _loopment.times =
            static_cast<unsigned long long>(loop.at("times").as_integer());
        _loopment.delay = loop.at("delay").as_floating();

        _property.loop = _loopment;
        keys_properties.push_back(_property);
    }
}
