#include <macro_manager.hpp>
#include <macro.hpp>
#include <config.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

namespace
{
    std::mutex g_mtx;
    // key -> (its pre-parsed macro_script, its loopment); guarded by g_mtx
    struct script_entry
    {
        macro_script script;
        loopment loop;
    };
    std::unordered_map<keyboard::keys, script_entry> g_scripts;

    // key -> live worker thread object plus a "finished" flag.
    // The worker thread ONLY sets finished=true when it returns; it never
    // touches its own jthread object (joining yourself -> abort).
    // Cleanup of finished threads happens on the calling thread (next toggle
    // or shutdown), where joining is safe.
    struct running_macro
    {
        std::jthread th;
        std::atomic<bool> finished{false};
    };
    std::unordered_map<keyboard::keys, std::unique_ptr<running_macro>> g_running;
} // namespace

namespace macro
{
    void register_macro(keyboard::keys key, const macro_script &script, const loopment &loop)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_scripts[key] = script_entry{script, loop};
    }

    void toggle(keyboard::keys key)
    {
        std::lock_guard<std::mutex> lk(g_mtx);

        auto sit = g_scripts.find(key);
        if (sit == g_scripts.end())
            return; // not a hotkey

        auto rit = g_running.find(key);
        if (rit != g_running.end())
        {
            running_macro *rm = rit->second.get();
            if (rm->finished.load())
            {
                // previous run already finished -> drop the dead thread object
                // (joining it here is safe: the OS thread has exited)
                g_running.erase(rit);
            }
            else
            {
                // still running -> toggle off
                rm->th.request_stop();
                return;
            }
        }

        // start a fresh worker for this key
        const macro_script script = sit->second.script; // copy for the worker
        const loopment loop = sit->second.loop;
        auto rm = std::make_unique<running_macro>();
        running_macro *raw = rm.get();
        raw->th = std::jthread([script, loop, raw](std::stop_token st)
                               {
            run_macro_script(st, script, loop);
            // signal completion; do NOT touch g_running / raw->th here
            raw->finished.store(true);
        });
        g_running.emplace(key, std::move(rm));
    }

    void shutdown()
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        for (auto &[key, rm] : g_running)
            rm->th.request_stop();
        // destroy after requesting stop; jthread join happens here on the
        // calling thread while we still hold the lock — workers never take
        // this lock, so there is no deadlock.
        g_running.clear();
    }
} // namespace macro