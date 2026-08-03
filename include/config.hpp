#pragma once
// we will store these props in a file in the future
// thus, just temporally store them in .c

#include <string>
#include <vector>

enum class script_type;

struct loopment;

struct key_property;

// unit: px
extern int mainwindow_height;

// unit: px
extern int mainwindow_width;

extern const std::string mainwindow_title;

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