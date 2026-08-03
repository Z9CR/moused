#pragma once

// bind mouse/keyboard enums as Lua globals (mouse_btn, wheel_rotation, keys)
// run after luaL_newstate() + luaL_openlibs(), before loading any script
struct lua_State;
void register_script_enums(lua_State *L);
