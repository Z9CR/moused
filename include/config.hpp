#pragma once
// we will store these props in a file in the future
// thus, just temporally store them in .c

// unit: px
extern int mainwindow_height;

// unit: px
extern int mainwindow_width;

extern const char* mainwindow_title;

// windows: `%APPDATA%/moused/`'s specific path
// linux&bsds: `~/.config/moused/`'s specific~
// macos: `~/Library/moused/`
extern char platform_cfg_dir[1024];

extern const char* config_name;

// when smooth moving,
// prog will slice the path to dest into pieces,
// and the value below is how long will stay in per pieces
// unit: ms
extern const int smoothmv_frametime;

// utils
// get `const char* platform_cfg_dir`
// Fetch the Roaming `%%APPDATA%%\`(win), `~/moused/`(linux||bsds), `~/Library/moused/`(macos),
// then write into platform_cfg_dir
// attention: the `\` or `/` is included in the tail
// run it when init
// throws std::runtime_error on failure
void init_cfg_dir_properties();

void touch_config_file(const char* parent_path, const char* name);


// declare luas
struct lua_State; 
void register_script_enums(lua_State* L);
