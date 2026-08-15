#include <adapter.hpp>
#include <algorithm>
#include <config.hpp>
#include <format>
#include <macro.hpp>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {
// command name -> command_type lookup (mirrors COMMAND_LIST in macro.hpp)
const struct {
    std::string_view name;
    command_type type;
} command_names[] = {
    {"translate", command_type::translate},
    {"move_to", command_type::move_to},
    {"click", command_type::click},
    {"press", command_type::press},
    {"release", command_type::release},
    {"wheel", command_type::wheel},
};

// extract the double at index i from args, falling back to 0 when absent
double arg_at(const command& cmd, std::size_t i) {
    return i < cmd.args.size() ? cmd.args[i] : 0.0;
}

// render a single arg: buttons / wheel rotations come back as their quoted
// name ('LMB', 'WU'), everything else as a number. The button and wheel
// enums share values (LMB == WU == 0), so look up per command kind.
std::string format_arg(command_type type, std::size_t index, double value) {
    if (index != 0) return std::format("{:g}", value);
    const int int_val = static_cast<int>(value);
    switch (type) {
        case command_type::click:
        case command_type::press:
        case command_type::release: {
#define BTN_ITEM(n, v) {#n, static_cast<int>(v)},
            static const struct {
                std::string_view name;
                int value;
            } btns[] = {MOUSE_BTN_LIST(BTN_ITEM)};
#undef BTN_ITEM
            for (const auto& e : btns)
                if (e.value == int_val) return std::string(e.name);
            break;
        }
        case command_type::wheel: {
#define WHEEL_ITEM(n, v) {#n, static_cast<int>(v)},
            static const struct {
                std::string_view name;
                int value;
            } wheels[] = {WHEEL_ROTATION_LIST(WHEEL_ITEM)};
#undef WHEEL_ITEM
            for (const auto& e : wheels)
                if (e.value == int_val) return std::string(e.name);
            break;
        }
        default:
            break;
    }
    return std::format("{:g}", value);
}
}  // namespace

std::optional<command_type> command_type_from_string(std::string_view name) {
    for (const auto& e : command_names)
        if (e.name == name) return e.type;
    return std::nullopt;
}

std::string_view command_type_to_string(command_type type) {
    for (const auto& e : command_names)
        if (e.type == type) return e.name;
    return "unknown";
}

std::string format_macro_script(const macro_script& script) {
    std::string out;
    for (std::size_t i = 0; i < script.size(); ++i) {
        const auto& cmd = script[i];
        if (i > 0) out += "\n";
        out += std::format("{{ cmd = '{}', args = [",
                           command_type_to_string(cmd.type));
        for (std::size_t j = 0; j < cmd.args.size(); ++j) {
            if (j > 0) out += ", ";
            out += format_arg(cmd.type, j, cmd.args[j]);
        }
        out += std::format("], delay = {:g} }}", cmd.delay);
    }
    return out;
}

void dispatch_command(const command& cmd) {
    switch (cmd.type) {
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
            mouse::click(static_cast<mouse::mouse_btns>(
                static_cast<int>(arg_at(cmd, 0))));
            break;
        case command_type::press:
            mouse::press(static_cast<mouse::mouse_btns>(
                static_cast<int>(arg_at(cmd, 0))));
            break;
        case command_type::release:
            mouse::release(static_cast<mouse::mouse_btns>(
                static_cast<int>(arg_at(cmd, 0))));
            break;
        case command_type::wheel:
            mouse::wheel(static_cast<mouse::wheel_rotations>(
                             static_cast<int>(arg_at(cmd, 0))),
                         arg_at(cmd, 1));
            break;
    }
}

bool wait_for_or_stop(std::stop_token st, double ms) {
    if (ms <= 0.0) return !st.stop_requested();

    // Poll in small slices so a stop request is honored promptly, without
    // relying on a global shared condition_variable (which could otherwise
    // be disturbed by concurrent workers waiting on the same cv). A 4 ms
    // slice matches the smooth-move frame time, so a shutdown join waits at
    // most ~4 ms for any worker that is inside this function.
    const double total = std::max(ms, 4.0);
    double waited = 0.0;
    while (waited < total) {
        if (st.stop_requested()) return false;
        const double slice = std::min(4.0, total - waited);
        std::this_thread::sleep_for(
            std::chrono::duration<double, std::milli>(slice));
        waited += slice;
    }
    return !st.stop_requested();
}

void run_macro_script(std::stop_token st, const macro_script& script,
                      const struct loopment& loop) {
    auto run_once = [&]() -> bool {
        for (const auto& cmd : script) {
            if (st.stop_requested()) return false;
            if (!wait_for_or_stop(st, cmd.delay)) return false;
            dispatch_command(cmd);
        }
        return true;
    };

    if (!loop.enabled) {
        run_once();
        return;
    }

    if (loop.times == static_cast<unsigned long long>(-1)) {
        // infinite loop: repeat the whole script until stopped
        while (!st.stop_requested()) {
            if (!run_once()) return;
            if (!wait_for_or_stop(st, loop.delay)) return;
        }
        return;
    }

    for (unsigned long long i = 0; i < loop.times; ++i) {
        if (st.stop_requested()) return;
        if (!run_once()) return;
        if (!wait_for_or_stop(st, loop.delay)) return;
    }
}