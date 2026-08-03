#include <adapter.hpp>
#include <config.hpp>
#include <macro.hpp>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>
#include <format>
#include <thread>

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace
{
    // fetch a field from the table at the given index as a Lua number (double)
    double read_num_field(lua_State *L, int idx, const char *field)
    {
        lua_getfield(L, idx, field);
        if (!lua_isnumber(L, -1))
        {
            std::string msg = std::format("moused: expected number field `{}` in script command table", field);
            lua_pop(L, 1);
            throw std::runtime_error(msg);
        }
        double v = lua_tonumber(L, -1);
        lua_pop(L, 1);
        return v;
    }

    // extract the double at index i from args, falling back to 0 when absent
    double arg_at(const command &cmd, std::size_t i)
    {
        return i < cmd.args.size() ? cmd.args[i] : 0.0;
    }
} // namespace

macro_script parse_lua_result(lua_State *L)
{
    macro_script script;
    int top = lua_gettop(L);

    // The `run()` function should have left a single return value on the stack.
    if (top < 1 || !lua_istable(L, -1))
    {
        throw std::runtime_error("moused: hotkey script must return a table of commands");
    }

    try
    {
        // iterate the array part of the returned table (1-based)
        std::size_t n = static_cast<std::size_t>(lua_rawlen(L, -1));
        for (std::size_t i = 1; i <= n; ++i)
        {
            lua_rawgeti(L, -1, static_cast<lua_Integer>(i));
            if (!lua_istable(L, -1))
            {
                throw std::runtime_error(std::format("moused: element #{} of script result is not a table", i));
            }

            command cmd;
            cmd.type = static_cast<command_type>(static_cast<int>(read_num_field(L, -1, "cmd")));
            cmd.delay = read_num_field(L, -1, "delay");

            // read `args` — an array of Lua numbers (doubles)
            lua_getfield(L, -1, "args");
            if (!lua_istable(L, -1))
            {
                throw std::runtime_error(std::format("moused: command #{} is missing a table `args`", i));
            }
            std::size_t args_len = static_cast<std::size_t>(lua_rawlen(L, -1));
            cmd.args.reserve(args_len);
            for (std::size_t a = 1; a <= args_len; ++a)
            {
                lua_rawgeti(L, -1, static_cast<lua_Integer>(a));
                if (!lua_isnumber(L, -1))
                {
                    throw std::runtime_error(std::format("moused: arg #{} of command #{} is not a number", a, i));
                }
                cmd.args.push_back(lua_tonumber(L, -1));
                lua_pop(L, 1);
            }
            lua_pop(L, 1); // args table

            lua_pop(L, 1); // command table
            script.push_back(std::move(cmd));
        }

        if (script.empty())
            throw std::runtime_error("moused: hotkey script returned an empty command list");
    }
    catch (...)
    {
        // restore the stack so repeated calls on the same state don't accumulate
        lua_settop(L, top);
        throw;
    }

    // pop the returned table; stack is back to its pre-call height
    lua_pop(L, 1);
    return script;
}

void dispatch_command(const command &cmd)
{
    switch (cmd.type)
    {
    case command_type::translate:
        if (cmd.args.size() >= 3)
            mouse::translate(static_cast<angle>(arg_at(cmd, 0)),
                             static_cast<int>(arg_at(cmd, 1)),
                             static_cast<unsigned int>(arg_at(cmd, 2)));
        else
            mouse::translate(static_cast<angle>(arg_at(cmd, 0)),
                             static_cast<int>(arg_at(cmd, 1)));
        break;
    case command_type::move_to:
        if (cmd.args.size() >= 3)
            mouse::move_to(static_cast<unsigned int>(arg_at(cmd, 0)),
                           static_cast<unsigned int>(arg_at(cmd, 1)),
                           static_cast<unsigned int>(arg_at(cmd, 2)));
        else
            mouse::move_to(static_cast<unsigned int>(arg_at(cmd, 0)),
                           static_cast<unsigned int>(arg_at(cmd, 1)));
        break;
    case command_type::click:
        mouse::click(static_cast<mouse::mouse_btns>(static_cast<int>(arg_at(cmd, 0))));
        break;
    case command_type::press:
        mouse::press(static_cast<mouse::mouse_btns>(static_cast<int>(arg_at(cmd, 0))));
        break;
    case command_type::release:
        mouse::release(static_cast<mouse::mouse_btns>(static_cast<int>(arg_at(cmd, 0))));
        break;
    case command_type::wheel:
        mouse::wheel(static_cast<mouse::wheel_rotations>(static_cast<int>(arg_at(cmd, 0))),
                     arg_at(cmd, 1));
        break;
    }
}

bool wait_for_or_stop(std::stop_token st, double ms)
{
    if (ms <= 0.0)
        return !st.stop_requested();

    static std::mutex mtx;
    static std::condition_variable_any cv;
    std::unique_lock<std::mutex> lk(mtx);
    // wait_for returns cv_status::timeout on full delay, no_timeout if notified.
    return !cv.wait_for(lk, st, std::chrono::duration<double, std::milli>(ms),
                        [] { return false; });
}

void run_macro_script(std::stop_token st, const macro_script &script,
                      const struct loopment &loop)
{
    auto run_once = [&]() -> bool
    {
        for (const auto &cmd : script)
        {
            if (st.stop_requested())
                return false;
            if (!wait_for_or_stop(st, cmd.delay))
                return false;
            dispatch_command(cmd);
        }
        return true;
    };

    if (!loop.enabled)
    {
        run_once();
        return;
    }

    if (loop.times == static_cast<unsigned long long>(-1))
    {
        // infinite loop: repeat the whole script until stopped
        while (!st.stop_requested())
        {
            if (!run_once())
                return;
            if (!wait_for_or_stop(st, loop.delay))
                return;
        }
        return;
    }

    for (unsigned long long i = 0; i < loop.times; ++i)
    {
        if (st.stop_requested())
            return;
        if (!run_once())
            return;
        if (!wait_for_or_stop(st, loop.delay))
            return;
    }
}