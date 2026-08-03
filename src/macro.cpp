#include <macro.hpp>
#include <stdexcept>
#include <string>
#include <format>

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