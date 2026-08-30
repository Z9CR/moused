#include <adapter.hpp>
#include <algorithm>
#include <cctype>
#include <config.hpp>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <macro_manager.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <toml.hpp>
#include <utils.hpp>
#include <vector>
#include <default_conf.hpp>

namespace {
// strip leading/trailing whitespace (space, \t, \r, \n, \v, \f). TOML
// multiline strings (`"""..."""`) keep indentation and trailing newlines,
// which are almost always accidental for a file path, so we normalize them
// here.
std::string trim_ws(const std::string& s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    const auto first = std::find_if(s.begin(), s.end(), not_space);
    const auto last = std::find_if(s.rbegin(), s.rend(), not_space).base();
    if (first >= last) return {};
    return std::string(first, last);
}

// Resolve a possibly-relative file path (a `replay` type `val`) against the
// config directory — mirrors the old `type = 'file'` script path handling.
// Empty input stays empty (no `onInterrupt`).
std::string resolve_config_path(const std::string& p) {
    if (p.empty()) return {};
    std::filesystem::path path(p);
    if (path.is_relative())
        path = std::filesystem::path(platform_cfg_dir) / path;
    return path.string();
}

// convert a TOML number (integer or floating) to double
double to_double(const toml::value& v) {
    if (v.is_integer()) return static_cast<double>(v.as_integer());
    if (v.is_floating()) return v.as_floating();
    throw std::runtime_error("moused: expected a number in config file");
}

// resolve a string `args` element ('LMB', 'WU', ...) to its enum value
double parse_arg_name(const std::string& name) {
#define ARG_NAME_ITEM(n, v) {#n, static_cast<double>(v)},
    static const struct {
        std::string_view name;
        double value;
    } arg_names[] = {MOUSE_BTN_LIST(ARG_NAME_ITEM) WHEEL_ROTATION_LIST(ARG_NAME_ITEM)};
#undef ARG_NAME_ITEM
    for (const auto& e : arg_names)
        if (e.name == name) return e.value;
    throw std::runtime_error(
        fmt::format("moused: unknown command argument `{}` in config file", name));
}

// convert a single `args` element (number or quoted name) to a double
double parse_arg(const toml::value& v) {
    if (v.is_string()) return parse_arg_name(v.as_string());
    if (v.is_integer()) return static_cast<double>(v.as_integer());
    if (v.is_floating()) return v.as_floating();
    throw std::runtime_error(
        "moused: command argument must be a number or a quoted name "
        "('LMB', 'WU', ...)");
}

// parse a [key.onActive].val / [key.onInterrupt].val array of instruction
// tables into a macro_script:
//     val = [{ cmd = 'translate', args = [0, 32, 10], delay = 0 }, ...]
// `delay` is optional and defaults to 0.0; `args` may also be a bare scalar
// (`args = 'LMB'` is equivalent to `args = ['LMB']`).
macro_script parse_command_list(const toml::value& val) {
    macro_script script;
    if (!val.is_array())
        throw std::runtime_error(
            "moused: `val` must be an array of instruction tables "
            "{ cmd = '...', args = [...], delay = <double>? }");
    for (const auto& instr : val.as_array()) {
        if (!instr.is_table())
            throw std::runtime_error(
                "moused: every instruction in `val` must be a table "
                "{ cmd = '...', args = [...], delay = <double>? }");
        const auto& tbl = instr.as_table();
        auto it = tbl.find("cmd");
        if (it == tbl.end() || !it->second.is_string())
            throw std::runtime_error(
                "moused: every instruction needs `cmd = 'name'`");
        auto cmd_type = command_type_from_string(it->second.as_string());
        if (!cmd_type)
            throw std::runtime_error(fmt::format(
                "moused: unknown command `{}` in config file",
                it->second.as_string()));
        command cmd;
        cmd.type = *cmd_type;
        cmd.delay = 0.0;
        if (auto d = tbl.find("delay"); d != tbl.end())
            cmd.delay = to_double(d->second);
        if (auto a = tbl.find("args"); a != tbl.end()) {
            if (a->second.is_array())
                for (const auto& e : a->second.as_array())
                    cmd.args.push_back(parse_arg(e));
            else
                cmd.args.push_back(parse_arg(a->second));
        }
        script.push_back(std::move(cmd));
    }
    return script;
}

// reverse map an enum arg (button / wheel rotation) back to its config name;
// returns std::nullopt for anything that is not a known name.
std::optional<std::string> enum_arg_to_string(command_type type, int value) {
    if (type == command_type::wheel) {
#define WHEEL_ITEM(n, v) {#n, static_cast<int>(v)},
        static const struct {
            std::string_view name;
            int value;
        } wheel_names[] = {WHEEL_ROTATION_LIST(WHEEL_ITEM)};
#undef WHEEL_ITEM
        for (const auto& e : wheel_names)
            if (e.value == value) return std::string(e.name);
        return std::nullopt;
    }
#define BTN_ITEM(n, v) {#n, static_cast<int>(v)},
    static const struct {
        std::string_view name;
        int value;
    } btn_names[] = {MOUSE_BTN_LIST(BTN_ITEM)};
#undef BTN_ITEM
    for (const auto& e : btn_names)
        if (e.value == value) return std::string(e.name);
    return std::nullopt;
}

// serialize a macro_script back into a TOML array of instruction tables
// (used by flash_into_config)
toml::value script_to_toml(const macro_script& script) {
    toml::array arr;
    for (const auto& cmd : script) {
        toml::table instr;
        instr["cmd"] = std::string(command_type_to_string(cmd.type));
        toml::array args;
        const bool named_first =
            cmd.type == command_type::click || cmd.type == command_type::press ||
            cmd.type == command_type::release || cmd.type == command_type::wheel;
        for (std::size_t i = 0; i < cmd.args.size(); ++i) {
            if (i == 0 && named_first) {
                if (auto s =
                        enum_arg_to_string(cmd.type, static_cast<int>(cmd.args[0])))
                    args.push_back(toml::value(*s));
                else
                    args.push_back(toml::value(cmd.args[0]));
            } else {
                args.push_back(toml::value(cmd.args[i]));
            }
        }
        instr["args"] = std::move(args);
        instr["delay"] = cmd.delay;
        arr.push_back(std::move(instr));
    }
    return toml::value(std::move(arr));
}
}  // namespace

// fallbacks
int smoothmv_frametime = 4;

const std::string mainwindow_title = "moused";

// window size upper bound, falls back to 0 = "no max size"
int mainwindow_max_width = 0;
int mainwindow_max_height = 0;

// parsed from [global].silent_launch; true = start to tray without showing
// the main window
bool silent_launch = true;

// parsed from [global].language; "system" (follow the OS UI language),
// "en_US", "zh_CN", ... — values must match langTable in src/ui.cpp
std::string ui_language{"system"};

// global config directory path, static lifetime
std::string platform_cfg_dir;

// cfg name
const std::string config_name = "config.toml";

// since `platform_cfg_dir` now is not inited, so we need to get path this way:
// std::filesystem::path cfg =
//    std::filesystem::path(platform_cfg_dir) / config_name;

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
// MacOS: `~/Library/moused/`
#include <cstdlib>

// Fetch `~/Library/moused/`, write into platform_cfg_dir
// Uses $HOME, which is always the current user on macOS (no pkexec/sudo
// elevation, unlike the Linux/BSD path).
void init_cfg_dir_properties() {
    const char* home = std::getenv("HOME");
    if (home) {
        std::filesystem::path cfg{home};
        cfg /= "Library";
        cfg /= "moused";
        platform_cfg_dir = cfg.string();
        return;
    }
    throw std::runtime_error("failed to fetch config path");
}
#else
// unk sys
#endif
#pragma endregion

// this var to store properties
std::vector<key_property> keys_properties{};

void flash_into_config() {
    std::filesystem::path cfg =
        std::filesystem::path(platform_cfg_dir) / config_name;
    toml::value conf = toml::parse(cfg.string());
    conf["global"]["smooth_frametime_ms"] = smoothmv_frametime;
    conf["global"]["max_window_width"] = mainwindow_max_width;
    conf["global"]["max_window_height"] = mainwindow_max_height;
    conf["global"]["silent_launch"] = silent_launch;
    conf["global"]["language"] = ui_language;

    // gen keyboard::keys -> string name map (mirrors read_from_config)
    // `keys = [...]` in config stores *names* (["LEFT_CONTROL","L"]), not enum
    // values, so flash must convert back to names before writing.
    const struct {
        int k;
        const char* v;
    } table[] = {
#define KEYS_ITEM(name, value) {value, #name},
        KEYS_LIST(KEYS_ITEM)
#undef KEYS_ITEM
    };
    auto key_to_name = [&](keyboard::keys key) -> std::string {
        for (const auto& item : table)
            if (static_cast<keyboard::keys>(item.k) == key) return item.v;
        return "NONE";
    };

    for (const auto& key : keys_properties) {
        std::vector<std::string> key_names;
        key_names.reserve(key.keys.size());
        for (auto k : key.keys) key_names.push_back(key_to_name(k));
        conf[key._table_name]["keys"] = key_names;
        conf[key._table_name]["enabled"] = key.enabled;
        conf[key._table_name]["type"] =
            key.type == script_type::in_line ? "inline" : "replay";
        if (key.type == script_type::replay) {
            conf[key._table_name]["onActive"]["val"] = key.val;
            if (key.has_interrupt)
                conf[key._table_name]["onInterrupt"]["val"] =
                    key.interrupt_val;
        } else {
            conf[key._table_name]["onActive"]["val"] = script_to_toml(key.active);
            if (key.has_interrupt)
                conf[key._table_name]["onInterrupt"]["val"] =
                    script_to_toml(key.interrupt);
        }
        conf[key._table_name]["loop"]["enabled"] = key.loop.enabled;
        conf[key._table_name]["loop"]["times"] = key.loop.times;
        conf[key._table_name]["loop"]["delay"] = key.loop.delay;
    }
    // Write back to file.
    // NOTE: std::ofstream does NOT throw on open/write failure by default —
    // it only sets failbit/badbit. So we must check the stream state
    // explicitly and translate failures into exceptions for the caller
    // (ui.cpp wraps flash_into_config() in try/catch to undo the toggle).
    std::ofstream of{cfg};
    if (!of) {
        throw std::runtime_error(fmt::format(
            "moused: failed to open config file `{}`", cfg.string()));
    }
    of << toml::format(conf);
    if (!of) {
        throw std::runtime_error(fmt::format(
            "moused: failed to write config file `{}`", cfg.string()));
    }
    of.close();
    if (!of) {
        throw std::runtime_error(fmt::format(
            "moused: failed to flush config file `{}`", cfg.string()));
    }
}

void touch_config_file(const std::string& parent_path,
                       const std::string& name) {
    // if the file already exists, do nothing
    std::filesystem::path cfg(parent_path);
    cfg /= name;
    if (std::filesystem::exists(cfg)) {
        return;
    }
    // flash defalut config file into 
    std::ofstream out(cfg);
    if (!out) {
        throw std::runtime_error("failed to create config file");
    }
    out << default_conf_data;
}

void read_from_config() {
    std::filesystem::path cfg(platform_cfg_dir);
    cfg /= config_name;
    toml::value _props = toml::parse(cfg.string());
    // reset to defaults before re-parsing, so repeated calls don't accumulate
    // fallbacks
    smoothmv_frametime = 4;
    silent_launch = false;
    ui_language = "system";
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
            silent_launch = toml::find_or(val, "silent_launch", false);
            ui_language =
                toml::find_or(val, "language", std::string("system"));
            continue;
        }
        // every profile of a key should be a table
        if (!val.is_table()) {
            throw std::runtime_error(fmt::format(
                "moused: error occurred when parsing config file `{}`",
                cfg.string()));
        }
        // sub table of the key
        const auto& key_profile = val.as_table();
        // 2 structures to save profile temporally
        loopment _loopment{};
        key_property _property{};

        _property._table_name = key.data();
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
            throw std::runtime_error(fmt::format(
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
                    fmt::format("moused: unknown key `{}` in config file `{}`",
                                n, cfg.string()));
            _property.keys.push_back(k);
        }
        if (_property.keys.empty())
            throw std::runtime_error(fmt::format(
                "moused: empty keys array in config file `{}`", cfg.string()));
        /*
        [key]
        enabled = <bool>
        type = 'inline' or 'replay'
        [key.onActive]                  // required
        val = [...instruction tables]   // type = 'inline'
            | "path to replay file"     // type = 'replay'
        [key.onInterrupt]               // omittable, same shape as onActive
        [key.loop]
        enabled = <bool>
        times = <int>
        delay = <double>
        */
        _property.enabled = key_profile.at("enabled").as_boolean();
        const std::string type_str = key_profile.at("type").as_string();
        if (type_str == "inline")
            _property.type = script_type::in_line;
        else if (type_str == "replay")
            _property.type = script_type::replay;
        else
            throw std::runtime_error(fmt::format(
                "moused: unknown `type = '{}'` in section `{}` (expected "
                "'inline' or 'replay')",
                type_str, key));

        // [key.onActive] (required)
        if (!key_profile.contains("onActive") ||
            !key_profile.at("onActive").is_table())
            throw std::runtime_error(fmt::format(
                "moused: section `{}` is missing the required "
                "`[<name>.onActive]` table",
                key));
        const auto& on_active = key_profile.at("onActive").as_table();
        const auto& on_active_val = on_active.at("val");

        // [key.onInterrupt] (omittable)
        auto interrupt_it = key_profile.find("onInterrupt");
        if (interrupt_it != key_profile.end() && !interrupt_it->second.is_table())
            throw std::runtime_error(fmt::format(
                "moused: `[<name>.onInterrupt]` in section `{}` must be a "
                "table",
                key));

        if (_property.type == script_type::in_line) {
            _property.active = parse_command_list(on_active_val);
            if (interrupt_it != key_profile.end()) {
                _property.has_interrupt = true;
                _property.interrupt =
                    parse_command_list(interrupt_it->second.as_table().at("val"));
            } else {
                // [key.onInterrupt] omitted -> fall back to onActive
                _property.interrupt = _property.active;
            }
        } else {
            // replay: reuse the old `type = 'file'` path handling — resolve
            // the (relative) replay path against the config dir and keep the
            // resolved path in `val`; actual replay execution is future work
            if (!on_active_val.is_string())
                throw std::runtime_error(fmt::format(
                    "moused: replay section `{}` needs `val = \"path to "
                    "replay file\"`",
                    key));
            _property.val =
                resolve_config_path(trim_ws(on_active_val.as_string()));
            if (interrupt_it != key_profile.end()) {
                _property.has_interrupt = true;
                const auto& iv = interrupt_it->second.as_table().at("val");
                if (iv.is_string())
                    _property.interrupt_val =
                        resolve_config_path(trim_ws(iv.as_string()));
            } else {
                // [key.onInterrupt] omitted -> fall back to onActive
                _property.interrupt_val = _property.val;
            }
        }

        // [key.loop] sub table
        const auto& loop = key_profile.at("loop").as_table();
        _loopment.enabled = loop.at("enabled").as_boolean();
        _loopment.times =
            static_cast<unsigned long long>(loop.at("times").as_integer());
        _loopment.delay = to_double(loop.at("delay"));

        _property.loop = _loopment;
        keys_properties.push_back(_property);
    }
}

// Register every enabled hotkey's macro. Inline macros were already parsed
// into `key_property::active` by read_from_config(); here we cache them for
// the runtime. Replay macros are planned content — only their file path is
// parsed (key_property::val) — so they are skipped for now.
// The registered set is cleared first so it always mirrors `enabled`: this
// matters when the UI re-calls warmup after toggling a macro's enabled state.
void warmup_macros() {
    macro::clear_macros();
    for (const auto& prop : keys_properties) {
        if (!prop.enabled) continue;
        if (prop.type == script_type::replay) {
            log_msg("moused: replay macro for key %d is not supported yet "
                    "(replay file `%s`)\n",
                    static_cast<int>(prop.keys.front()), prop.val.c_str());
            continue;
        }
        try {
            macro::register_macro(prop.keys, prop.active, prop.loop);
        } catch (const std::exception& e) {
            log_msg("moused: failed to register macro for key %d: %s\n",
                    static_cast<int>(prop.keys.front()), e.what());
        }
    }
}
