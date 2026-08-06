#pragma once
// we will store these props in a file in the future
// thus, just temporally store them in .c

#include <adapter.hpp>
#include <string>
#include <vector>

// the type of a hotkey's bound script
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
    // combo key: at least one key; length > 1 means a modifier combination.
    // Parsed from the `keys = [...]` array; when absent, falls back to the
    // section name mapped via KEYS_LIST (single key, old format).
    std::vector<keyboard::keys> keys;
    bool enabled;
    script_type type;
    std::string code;
    loopment loop;
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

extern std::vector<key_property> keys_properties;

// utils
// get `std::string platform_cfg_dir`
// Fetch the Roaming `%%APPDATA%%\`(win), `~/moused/`(linux||bsds), `~/Library/moused/`(macos),
// then write into platform_cfg_dir
// attention: the `\` or `/` is included in the tail
// run it when init
// throws std::runtime_error on failure
void init_cfg_dir_properties();

void touch_config_file(const std::string &parent_path, const std::string &name);

void refresh_config();