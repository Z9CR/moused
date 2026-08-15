#pragma once
// A hotkey macro is a list of instructions declared natively in the TOML
// config (no scripting language involved):
//
//     [CTRL_L.onActive]
//     val = [
//       { cmd = 'translate', args = [0, 32, 10], delay = 0 },
//       { cmd = 'wheel',     args = ['WU', 10],  delay = 0.1 },
//     ]
//
// Each instruction waits `delay` milliseconds BEFORE executing, then runs
// `cmd` with the given `args`. All `args` are stored as doubles; quoted
// names such as 'LMB' / 'WU' are resolved to their enum values at config
// parse time (see config.cpp), and the execution side casts the doubles to
// whatever type each adapter function requires.

#include <cstddef>
#include <enums_list.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct loopment;  // forward declaration only; macro.cpp sees the full
                  // definition

// command names -> command_type values, one enumerator per adapter command
#define COMMAND_LIST(ITEM) \
    ITEM(translate, 0)     \
    ITEM(move_to, 1)       \
    ITEM(click, 2)         \
    ITEM(press, 3)         \
    ITEM(release, 4)       \
    ITEM(wheel, 5)

enum class command_type {
#define COMMAND_ITEM(name, value) name = value,
    COMMAND_LIST(COMMAND_ITEM)
#undef COMMAND_ITEM
};

#undef COMMAND_LIST

struct command {
    command_type type;
    std::vector<double> args;  // doubles, cast to target types at dispatch
    double delay = 0.0;        // ms to wait before this instruction
};

// one macro = an ordered list of instructions
using macro_script = std::vector<command>;

/// Map a config command name (`'translate'`) to its command_type.
/// Returns std::nullopt when the name is unknown.
std::optional<command_type> command_type_from_string(std::string_view name);

/// Inverse of command_type_from_string (used by flash / display).
std::string_view command_type_to_string(command_type type);

/// Render a macro_script as one `{ cmd = '...', args = [...], delay = ... }`
/// line per instruction (used by the editor dialog).
std::string format_macro_script(const macro_script& script);

#include <stop_token>

/// Dispatch a single command to the platform adapter, casting the stored
/// doubles to the types each adapter function expects. Missing trailing
/// arguments fall back to 0.
void dispatch_command(const command& cmd);

/// Wait for `ms` milliseconds, aborting early when `st` receives a stop
/// request. Returns true if the full delay elapsed, false if stopped.
bool wait_for_or_stop(std::stop_token st, double ms);

/// Execute a macro_script on the calling thread. Each instruction first
/// waits its `delay` (interruptible), then dispatches. If `loop.enabled`,
/// the whole script repeats `loop.times` times (or forever when
/// `loop.times == -1`), pausing `loop.delay` ms between rounds.
/// Returns early as soon as a stop is requested.
void run_macro_script(std::stop_token st, const macro_script& script,
                      const struct loopment& loop);
