#pragma once
// we will store these props in a file in the future
// thus, just temporally store them in .c

// unit: px
extern const int mainwindow_height;

// unit: px
extern const int mainwindow_width;

// windows: `%APPDATA%`'s specific path
// linux&bsds: `~/.config/moused/`'s specific~
// macos: `~/Library/moused`
extern char platform_cfg_dir[];

// use char* to avoid huuuuuuge STL
// cfg_path should point to `${platform_cfg_dir}/moused/cfg`
extern const char *cfg_path;

// when smooth moving,
// prog will slice the path to dest into pieces,
// and the value below is how long will stay in per pieces
// unit: ms
extern const int smoothmv_frametime;

// utils
// get `const char* platform_cfg_dir`
// Fetch the Roaming AppData path, write into platform_cfg_dir
// run it when init
bool fetch_cfg_dir();