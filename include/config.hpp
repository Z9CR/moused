#pragma once
// we will store these props in a file in the future
// thus, just temporally store them in .c

#include <adapter.hpp>
#include <filesystem>
#include <macro.hpp>
#include <string>
#include <vector>

// the type of a hotkey's bound macro
enum class script_type { in_line = 0, replay = 1 };

struct loopment {
    bool enabled;
    // when `-1`, it will be casted into `18446744073709551615`
    unsigned long long times;
    // unit: ms
    double delay;
};

struct key_property {
    // combo key: at least one key; length > 1 means a modifier combination.
    // Parsed from the `keys = [...]` array; when absent, falls back to the
    // section name mapped via KEYS_LIST (single key, old format).
    std::vector<keyboard::keys> keys;
    bool enabled;
    script_type type;
    // `replay` type: resolved path to the replay file from
    // [key.onActive].val (relative paths are resolved against the config
    // dir). Empty for `in_line` type.
    std::string val;
    // `in_line` type: instructions parsed from [key.onActive].val.
    macro_script active;
    // `in_line` type: instructions parsed from the optional
    // [key.onInterrupt].val. When the section is omitted it falls back to
    // `active`.
    macro_script interrupt;
    // `replay` type: resolved path from the optional [key.onInterrupt].val;
    // falls back to `val` when the section is omitted.
    std::string interrupt_val;
    // true only when [key.onInterrupt] was explicitly present in the config
    // (flash uses this so it never writes the fallback out to the file).
    bool has_interrupt = false;
    loopment loop;
    // `_table_name` is for flash only
    std::string _table_name;
};

extern const std::string mainwindow_title;

// window size upper bound, parsed from [global].max_window_width/height
extern int mainwindow_max_width;

// unit: px
extern int mainwindow_max_height;

// windows: `%APPDATA%/moused/`'s specific path
// linux&bsds: `~/.config/moused/`'s specific~
// macos: `~/Library/moused/`
extern std::string platform_cfg_dir;

extern const std::string config_name;

// when smooth moving,
// prog will slice the path to dest into pieces,
// and the value below is how long will stay in per pieces
// unit: ms
// parsed from [global].smooth_frametime_ms,thus mutable at runtime
extern int smoothmv_frametime;

// parsed from [global].silent_launch: when true the app starts to the tray
// without showing the main window
extern bool silent_launch;

// parsed from [global].language: "system" (follow the OS UI language),
// "en_US", "zh_CN", ... — the values must match langTable in src/ui.cpp.
extern std::string ui_language;

extern std::vector<key_property> keys_properties;

// utils
// get `std::string platform_cfg_dir`
// Fetch the Roaming `%%APPDATA%%\`(win), `~/moused/`(linux||bsds),
// `~/Library/moused/`(macos), then write into platform_cfg_dir attention: the
// `\` or `/` is included in the tail run it when init throws std::runtime_error
// on failure
void init_cfg_dir_properties();

// flash `keys_properties` into config file
void flash_into_config();

// func to make sure config file exists
void touch_config_file(const std::string& parent_path, const std::string& name);

// read content from config file and write in `keys_properties`
void read_from_config();

// (re)register every enabled macro into the runtime macro manager. Called at
// startup and re-called from the UI after toggling a macro's enabled state,
// so an enable toggle takes effect at runtime without a restart.
void warmup_macros();