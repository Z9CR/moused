#include <macro_manager.hpp>
#include <macro.hpp>
#include <config.hpp>

#include <algorithm>
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
    // combo-sig -> (its pre-parsed macro_script, its loopment); guarded by g_mtx
    struct script_entry
    {
        macro_script script;
        loopment loop;
    };
    std::unordered_map<std::string, script_entry> g_scripts;

    // combo-sig -> live worker thread object plus a "finished" flag.
    // The worker thread ONLY sets finished=true when it returns; it never
    // touches its own jthread object (joining yourself -> abort).
    // Cleanup of finished threads happens on the calling thread (next toggle
    // or shutdown), where joining is safe.
    struct running_macro
    {
        std::jthread th;
        std::atomic<bool> finished{false};
    };
    std::unordered_map<std::string, std::unique_ptr<running_macro>> g_running;
} // namespace

namespace macro
{
    std::string combo_sig(const std::vector<keyboard::keys> &keys)
    {
        auto ks = keys;
        std::sort(ks.begin(), ks.end());
        std::string s;
        for (auto k : ks)
            s += std::to_string(static_cast<int>(k)) + ",";
        return s;
    }

    void register_macro(const std::vector<keyboard::keys> &keys,
                        const macro_script &script, const loopment &loop)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_scripts[combo_sig(keys)] = script_entry{script, loop};
    }

    void toggle(const std::vector<keyboard::keys> &keys)
    {
        const std::string sig = combo_sig(keys);
        std::lock_guard<std::mutex> lk(g_mtx);

        auto sit = g_scripts.find(sig);
        if (sit == g_scripts.end())
            return; // not a hotkey

        auto rit = g_running.find(sig);
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

        // start a fresh worker for this combo
        const macro_script script = sit->second.script; // copy for the worker
        const loopment loop = sit->second.loop;
        auto rm = std::make_unique<running_macro>();
        running_macro *raw = rm.get();
        raw->th = std::jthread([script, loop, raw](std::stop_token st)
                               {
            run_macro_script(st, script, loop);
            // signal completion; do NOT touch g_running / raw->th here
            raw->finished.store(true); });
        g_running.emplace(sig, std::move(rm));
    }

    void shutdown()
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        for (auto &[sig, rm] : g_running)
            rm->th.request_stop();
        // destroy after requesting stop; jthread join happens here on the
        // calling thread while we still hold the lock — workers never take
        // this lock, so there is no deadlock.
        g_running.clear();
    }
} // namespace macro