#pragma once
// Manages the lifecycle of running hotkey macros.
//
// Each hotkey is identified by a combo signature string (sorted key-list),
// so both single keys and modifier combinations map to one slot.
// Toggle semantics: pressing the combo starts its macro (one std::jthread),
// pressing it again requests the macro to stop.
// Different combos may run their macros concurrently.
//
// Scripts are pre-parsed once at startup (see parse_lua_result); at runtime
// only the cached macro_script is executed on the worker thread.

#include <adapter.hpp>   // keyboard::keys
#include <macro.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace macro
{
    /// Build a canonical, order-independent signature for a key combo.
    /// {"L","LEFT_CONTROL"} and {"LEFT_CONTROL","L"} both yield the same
    /// signature.
    std::string combo_sig(const std::vector<keyboard::keys> &keys);

    /// Register the pre-parsed script for a hotkey combo, together with its
    /// loop settings. Re-registering the same combo replaces its entry.
    void register_macro(const std::vector<keyboard::keys> &keys,
                        const macro_script &script, const loopment &loop);

    /// Toggle a hotkey combo: start its macro if not running, otherwise
    /// request the running instance to stop.
    void toggle(const std::vector<keyboard::keys> &keys);

    /// Request all running macros to stop and join their threads. Safe to
    /// call from the main thread during shutdown.
    void shutdown();
} // namespace macro
