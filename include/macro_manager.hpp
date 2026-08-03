#pragma once
// Manages the lifecycle of running hotkey macros.
//
// Each hotkey has a toggle semantics: pressing the key starts its macro
// (one std::jthread), pressing it again requests the macro to stop.
// Different keys may run their macros concurrently.
//
// Scripts are pre-parsed once at startup (see parse_lua_result); at runtime
// only the cached macro_script is executed on the worker thread.

#include <adapter.hpp>
#include <macro.hpp>
#include <concepts>
#include <mutex>
#include <stop_token>
#include <unordered_map>
#include <vector>

namespace macro
{
    /// Register the pre-parsed script for a hotkey, together with its loop
    /// settings. Re-registering the same key replaces its cached entry.
    void register_macro(keyboard::keys key, const macro_script &script, const loopment &loop);

    /// Toggle a hotkey: start its macro if not running, otherwise request
    /// the running instance to stop. Does nothing when the key has no
    /// registered script.
    void toggle(keyboard::keys key);

    /// Request all running macros to stop and join their threads. Safe to
    /// call from the main thread during shutdown.
    void shutdown();
} // namespace macro