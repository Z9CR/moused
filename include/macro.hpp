#pragma once
// Lua <-> C++ execution contract.
//
// A hotkey script (inline Lua code or a .lua file) must return an ARRAY of
// instruction-tables. Each element:
//     {cmd = <cmd enum>, args = {<double>, <double>, ...}, delay = <double>?}
//
// `cmd` is one of the enum values bound to the Lua global `cmd`
// (see script_binding.cpp / enums_list.hpp COMMAND_LIST).
//
// `delay` is optional and defaults to 0.0 — wait this many milliseconds
// BEFORE executing this instruction.
//
// All `args` values are read as Lua numbers (doubles); the C++ execution
// side static_casts them to the types each adapter function requires.

#include <enums_list.hpp>
#include <cstddef>
#include <vector>

// generated from COMMAND_LIST, one enumerator per adapter command
enum class command_type
{
#define COMMAND_ITEM(name, value) name = value,
    COMMAND_LIST(COMMAND_ITEM)
#undef COMMAND_ITEM
};

struct command
{
    command_type type;
    std::vector<double> args; // Lua numbers, cast to target types at dispatch
    double delay = 0.0;       // ms to wait before this instruction
};

// one macro = an ordered list of instructions
using macro_script = std::vector<command>;

struct lua_State;

/// Execute the Lua `run()` function bound to a hotkey and parse its return
/// value into a macro_script. The returned script is guaranteed to be
/// non-empty. `errors` are reported by throwing std::runtime_error.
/// @note L must be the Lua state that already has the script loaded and
///       whose stack contains the `run` function to call.
macro_script parse_lua_result(lua_State *L);