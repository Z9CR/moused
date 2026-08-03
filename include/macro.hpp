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

struct loopment; // forward declaration only; macro.cpp sees the full definition

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

#include <stop_token>

/// Dispatch a single command to the platform adapter, casting the stored
/// doubles to the types each adapter function expects. Missing trailing
/// arguments fall back to 0.
void dispatch_command(const command &cmd);

/// Wait for `ms` milliseconds, aborting early when `st` receives a stop
/// request. Returns true if the full delay elapsed, false if stopped.
bool wait_for_or_stop(std::stop_token st, double ms);

/// Execute a macro_script on the calling thread. Each instruction first
/// waits its `delay` (interruptible), then dispatches. If `loop.enabled`,
/// the whole script repeats `loop.times` times (or forever when
/// `loop.times == -1`), pausing `loop.delay` ms between rounds.
/// Returns early as soon as a stop is requested.
void run_macro_script(std::stop_token st, const macro_script &script,
                      const struct loopment &loop);
