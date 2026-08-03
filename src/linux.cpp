#include <adapter.hpp>
#include <config.hpp>
#include <utils.hpp>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/uinput.h>

#ifdef NULL
#undef NULL // Linux headers define NULL as `((void*)0)` (in C: __null, but we're coding with CPP) — we need it for keys::NONE
#endif

#pragma region define &include
using mouse::mouse_btns;
using mouse::wheel_rotations;

constexpr double PI = 3.14159265358979323846;

enum axis
{
    X = 0,
    Y = 1
};
#pragma endregion

#pragma region monitor init
// screen size — /dev/fb0, fallback 1920*1080
static int monitor_width = 1920;
static int monitor_height = 1080;

static void detect_screen()
{
    int fb = open("/dev/fb0", O_RDONLY);
    if (fb >= 0)
    {
        fb_var_screeninfo vi;
        if (ioctl(fb, FBIOGET_VSCREENINFO, &vi) == 0)
        {
            monitor_width = vi.xres;
            monitor_height = vi.yres;
        }
        close(fb);
    }
}

// cursor tracking — mirrors GetCursorPos()
// WARNING: if another program moves the cursor our state will be stale.
//          This is an inherent limitation of uinput (write-only).
static int tracked_x = 0;
static int tracked_y = 0;
#pragma endregion

#pragma region utils
// uinput singleton, root is required to open /dev/uinput
static int uinput_fd = -1;

static void uinput_cleanup()
{
    if (uinput_fd >= 0)
    {
        ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
        uinput_fd = -1;
    }
}

bool platform_uinput_setup()
{
    // inited or not
    if (uinput_fd >= 0)
        return true;

    // val -> monitor_width&&monitor_height
    detect_screen();

    // when the `uinput` isnt stub, open it as ReadOnly
    uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    // when `open()` occurs err, it'll ret negative val
    if (uinput_fd < 0)
    {
        log_msg("moused: cannot open /dev/uinput (need root). "
                "Run with sudo or as root.\n");
        return false;
    }

#pragma region event types
    // declare that can triggle key press event
    ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
    // declare that can triggle relative move event
    ioctl(uinput_fd, UI_SET_EVBIT, EV_REL);
    // declare that can triggle absolute move event
    ioctl(uinput_fd, UI_SET_EVBIT, EV_ABS);
    // declare that can triggle data syn ev
    ioctl(uinput_fd, UI_SET_EVBIT, EV_SYN);
#pragma endregion

#pragma region buttons
    // declare that can triggle ${key}
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_LEFT);   // LMB
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_RIGHT);  // RMB
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_MIDDLE); // MMB
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_SIDE);   // XB1
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_EXTRA);  // XB2
#pragma endregion

// (axis[si] -> axes[pl])
#pragma region axes
    // mouse wheel use relative axes
    ioctl(uinput_fd, UI_SET_RELBIT, REL_X);
    ioctl(uinput_fd, UI_SET_RELBIT, REL_Y);
    ioctl(uinput_fd, UI_SET_RELBIT, REL_WHEEL);

    // absolute axes (0 … 65535, same as Windows)
    ioctl(uinput_fd, UI_SET_ABSBIT, ABS_X);
    ioctl(uinput_fd, UI_SET_ABSBIT, ABS_Y);
#pragma endregion

#pragma region setup V-Devs
    // setup a virutal mouse named as `moused`
    uinput_setup usetup{};
    memset(&usetup, 0, sizeof(usetup));
    snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "moused");
    usetup.id.bustype = BUS_USB;   // pretend it is a USB dev
    usetup.id.vendor = 0x1145;   // just.. a
    usetup.id.product = 0x1919; // random num :)
    usetup.id.version = 0xF0C1c;  // F?CK
    // notify `ioctl` to setup our fake mouse which is named `moused`
    ioctl(uinput_fd, UI_DEV_SETUP, &usetup);
#pragma endregion

    uinput_abs_setup abs{};

// set the coordinate sys's X&Y's max&&min val as same as win,
// to synchronize Windows
#pragma region coordinate sys
    abs.code = ABS_X;
    abs.absinfo.minimum = 0;
    abs.absinfo.maximum = 65535;
    abs.absinfo.resolution = 1;
    ioctl(uinput_fd, UI_ABS_SETUP, &abs);

    // refer2 line 128
    abs.code = ABS_Y;
    abs.absinfo.minimum = 0;
    abs.absinfo.maximum = 65535;
    abs.absinfo.resolution = 1;
    ioctl(uinput_fd, UI_ABS_SETUP, &abs);
#pragma endregion

    if (ioctl(uinput_fd, UI_DEV_CREATE) < 0)
    {
        log_msg("moused: UI_DEV_CREATE failed\n");
        close(uinput_fd);
        uinput_fd = -1;
        return false;
    }

    atexit(uinput_cleanup);
    return true;
}

// emit helpers
static inline void emit(int type, int code, int val)
{
    input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = val;
    write(uinput_fd, &ev, sizeof(ev));
}

static inline void emit_sync()
{
    emit(EV_SYN, SYN_REPORT, 0);
}

// px to 0..65535  (same logic as Windows' MulDiv)
static int px2pos(int px, axis d)
{
    if (d == X)
        return (int)(((long long)px * 65535 + (monitor_width - 1) / 2) / (monitor_width - 1));
    else
        return (int)(((long long)px * 65535 + (monitor_height - 1) / 2) / (monitor_height - 1));
}

// no pos2px 'cause the /dev/uinput is write-only

// GetCursorPos() replacement
static void get_cursor(int &x, int &y)
{
    x = tracked_x;
    y = tracked_y;
}
#pragma endregion

namespace mouse
{
    /// @brief translate cursor by relative movement
    /// @param a the angle away from x+
    /// @param distant how many PXs will cursor translate
    void translate(angle a, int distant, unsigned int time_ms)
    {
        if (!platform_uinput_setup())
            return;

        // normalise angle
        if (a >= 360.0)
            while (a -= 360.0, a >= 360.0) {}
        if (a <= -360.0)
            while (a += 360.0, a <= -360.0) {}

        double rad = a * PI / 180.0;
        double total_dx = cos(rad) * distant;
        double total_dy = sin(rad) * distant;

        int frames = time_ms / smoothmv_frametime;
        if (frames < 1) frames = 1;

        double inc_x = total_dx / frames;
        double inc_y = total_dy / frames;
        double acc_x = 0.0, acc_y = 0.0;

        for (int i = 0; i < frames; ++i)
        {
            acc_x += inc_x;
            acc_y += inc_y;

            int step_x = static_cast<int>(acc_x);
            int step_y = static_cast<int>(acc_y);

            if (step_x != 0)
            {
                emit(EV_REL, REL_X, step_x);
                acc_x -= step_x;
            }
            if (step_y != 0)
            {
                emit(EV_REL, REL_Y, step_y);
                acc_y -= step_y;
            }
            if (step_x != 0 || step_y != 0)
                emit_sync();

            usleep(smoothmv_frametime * 1000);
        }

        // flush remaining subpixel remainder
        int rem_x = static_cast<int>(std::round(acc_x));
        int rem_y = static_cast<int>(std::round(acc_y));
        if (rem_x != 0) emit(EV_REL, REL_X, rem_x);
        if (rem_y != 0) emit(EV_REL, REL_Y, rem_y);
        if (rem_x != 0 || rem_y != 0) emit_sync();

        tracked_x += static_cast<int>(std::round(total_dx));
        tracked_y += static_cast<int>(std::round(total_dy));
    }

    /// @brief translate cursor by relative movement
    /// @param a the angle away from x+
    /// @param distant how many PXs will cursor translate
    void translate(angle a, int distant)
    {
        if (!platform_uinput_setup())
            return;

        // normalise angle
        if (a >= 360.0)
            while (a -= 360.0, a >= 360.0) {}
        if (a <= -360.0)
            while (a += 360.0, a <= -360.0) {}

        double rad = a * PI / 180.0;
        int dx = static_cast<int>(std::round(cos(rad) * distant));
        int dy = static_cast<int>(std::round(sin(rad) * distant));

        if (dx != 0) emit(EV_REL, REL_X, dx);
        if (dy != 0) emit(EV_REL, REL_Y, dy);
        if (dx != 0 || dy != 0) emit_sync();

        tracked_x += dx;
        tracked_y += dy;
    }

    /// @brief move the mouse to (x, y) (right = x+, down = y +)
    /// @param x: how many PXs away from left boarder
    /// @param y: how many PXs away from up boarder
    /// @param time_ms: time duration for the smooth movement, in ms
    void move_to(unsigned int x, unsigned int y, unsigned int time_ms)
    {
        if (!platform_uinput_setup())
            return;

        // get current position
        int start_x, start_y;
        get_cursor(start_x, start_y);

        // how many frames
        int frames = time_ms / smoothmv_frametime;
        if (frames < 1)
            frames = 1;

        // interpolate and send each frame
        for (int i = 1; i <= frames; ++i)
        {
            double t = static_cast<double>(i) / frames;
            int cur_x = start_x + (static_cast<int>(x) - start_x) * t;
            int cur_y = start_y + (static_cast<int>(y) - start_y) * t;

            emit(EV_ABS, ABS_X, px2pos(cur_x, X));
            emit(EV_ABS, ABS_Y, px2pos(cur_y, Y));
            emit_sync();

            usleep(smoothmv_frametime * 1000);
        }
        // in case float deviation
        move_to(x, y);
    }

    /// @brief move the mouse to (x, y) (right = x+, down = y +)
    /// @param x: how many PXs away from left boarder
    /// @param y: how many PXs away from up boarder
    void move_to(unsigned int x, unsigned int y)
    {
        if (!platform_uinput_setup())
            return;

        emit(EV_ABS, ABS_X, px2pos(x, X));
        emit(EV_ABS, ABS_Y, px2pos(y, Y));
        emit_sync();

        tracked_x = x;
        tracked_y = y;
    }

    /// @brief click the `btn`
    void click(mouse_btns btn)
    {
        if (!platform_uinput_setup())
            return;
        press(btn);
        release(btn);
    }

    /// @brief press the `btn`
    void press(mouse_btns btn)
    {
        if (!platform_uinput_setup())
            return;

        int code;
        switch (btn)
        {
        case LMB:
            code = BTN_LEFT;
            break;
        case RMB:
            code = BTN_RIGHT;
            break;
        case MMB:
            code = BTN_MIDDLE;
            break;
        case XB1:
            code = BTN_SIDE;
            break;
        case XB2:
            code = BTN_EXTRA;
            break;
        default:
            return;
        }
        emit(EV_KEY, code, 1);
        emit_sync();
    }

    /// @brief release the pressed `btn`
    void release(mouse_btns btn)
    {
        if (!platform_uinput_setup())
            return;

        int code;
        switch (btn)
        {
        case LMB:
            code = BTN_LEFT;
            break;
        case RMB:
            code = BTN_RIGHT;
            break;
        case MMB:
            code = BTN_MIDDLE;
            break;
        case XB1:
            code = BTN_SIDE;
            break;
        case XB2:
            code = BTN_EXTRA;
            break;
        default:
            return;
        }
        emit(EV_KEY, code, 0);
        emit_sync();
    }

    /// @brief routate the MMB for `scale`*Delta
    void wheel(wheel_rotations rotation, double scale)
    {
        if (!platform_uinput_setup())
            return;
        if (scale == 0.0)
            return;

        // In Linux, one REL_WHEEL unit ~= one notch.
        // Windows WHEEL_DELTA = 120.
        int delta;
        if (scale >= 1.0 || scale <= -1.0)
            delta = static_cast<int>(scale + (scale > 0.0 ? 0.5 : -0.5));
        else if (scale > 0.0)
            delta = 1;
        else
            delta = -1;

        if (rotation == WD)
            delta = -delta;

        emit(EV_REL, REL_WHEEL, delta);
        emit_sync();
    }
}

// as4 linux, we have root now so we can open /dev/...
// 4keyboard, we dont need to write /dev/uinput
//  instead reading /dev/input/event* (event* = keyboard*'s sig)

constexpr int MAX_KEYBOARDS = 16;
static int keyboards_fds[MAX_KEYBOARDS];
static int keyboard_count = 0;

static bool is_keyboard(int fd)
{
    unsigned long evbits;
    // Get the bitmask of supported event types
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits) < 0)
    {
        return false; // can't read — not a keyboard
    }

    // Check if EV_KEY (value 1) is supported
    if (evbits & (1 << EV_KEY))
    {
        return true; // Likely a keyboard or keypad
    }
    return false;
}

bool platform_keyboard_capture_setup()
{
    // open dir
    DIR *input_dir = opendir("/dev/input/");
    if (!input_dir) // when err
    {
        log_msg("error occured when opening /dev/input\n");
        return false;
    }

    dirent *entry{};
    // travel all the `input_dir`
    while ((entry = readdir(input_dir)) != nullptr)
    {
        // fliter event*
        if (strncmp(entry->d_name, "event", 5) != 0)
            continue;

        // get path — build safely with snprintf
        char fullpath[256];
        snprintf(fullpath, sizeof(fullpath), "/dev/input/%s", entry->d_name);
        // try to open
        int tmp_fd = open(fullpath, O_RDONLY);
        if (tmp_fd == -1)
            continue;

        if (is_keyboard(tmp_fd) && keyboard_count < MAX_KEYBOARDS)
        {
            keyboards_fds[keyboard_count++] = tmp_fd;
        }
        else
        {
            close(tmp_fd); // not a keyboard or limit reached — release fd
        }
    }
    closedir(input_dir);

    if (keyboard_count == 0)
    {
        log_msg("moused: no keyboard devices found in /dev/input/\n");
        return false;
    }
    return true;
}

using keys = keyboard::keys;

// map Linux input key codes to keys enum (mirrors Windows' vk2keys)
static keys linux_keycode_to_keys(int code)
{
    // numbers row: KEY_1(2)..KEY_9(10), KEY_0(11)
    if (code >= KEY_1 && code <= KEY_9)
        return static_cast<keys>('1' + (code - KEY_1));
    if (code == KEY_0)
        return keys::ZERO;
    switch (code)
    {
    // letters — Linux key codes are NOT alphabetical, must map individually
    case KEY_A: return keys::A;  case KEY_B: return keys::B;
    case KEY_C: return keys::C;  case KEY_D: return keys::D;
    case KEY_E: return keys::E;  case KEY_F: return keys::F;
    case KEY_G: return keys::G;  case KEY_H: return keys::H;
    case KEY_I: return keys::I;  case KEY_J: return keys::J;
    case KEY_K: return keys::K;  case KEY_L: return keys::L;
    case KEY_M: return keys::M;  case KEY_N: return keys::N;
    case KEY_O: return keys::O;  case KEY_P: return keys::P;
    case KEY_Q: return keys::Q;  case KEY_R: return keys::R;
    case KEY_S: return keys::S;  case KEY_T: return keys::T;
    case KEY_U: return keys::U;  case KEY_V: return keys::V;
    case KEY_W: return keys::W;  case KEY_X: return keys::X;
    case KEY_Y: return keys::Y;  case KEY_Z: return keys::Z;
    // punctuation & editing
    case KEY_MINUS:
        return keys::MINUS;
    case KEY_EQUAL:
        return keys::EQUAL;
    case KEY_BACKSPACE:
        return keys::BACKSPACE;
    case KEY_TAB:
        return keys::TAB;
    case KEY_LEFTBRACE:
        return keys::LEFT_BRACKET;
    case KEY_RIGHTBRACE:
        return keys::RIGHT_BRACKET;
    case KEY_ENTER:
        return keys::ENTER;
    case KEY_LEFTCTRL:
        return keys::LEFT_CONTROL;
    case KEY_SEMICOLON:
        return keys::SEMICOLON;
    case KEY_APOSTROPHE:
        return keys::APOSTROPHE;
    case KEY_GRAVE:
        return keys::GRAVE;
    case KEY_LEFTSHIFT:
        return keys::LEFT_SHIFT;
    case KEY_BACKSLASH:
        return keys::BACKSLASH;
    case KEY_COMMA:
        return keys::COMMA;
    case KEY_DOT:
        return keys::PERIOD;
    case KEY_SLASH:
        return keys::SLASH;
    case KEY_RIGHTSHIFT:
        return keys::RIGHT_SHIFT;
    case KEY_LEFTALT:
        return keys::LEFT_ALT;
    case KEY_SPACE:
        return keys::SPACE;
    case KEY_CAPSLOCK:
        return keys::CAPS_LOCK;
    // function keys
    case KEY_F1:
        return keys::F1;
    case KEY_F2:
        return keys::F2;
    case KEY_F3:
        return keys::F3;
    case KEY_F4:
        return keys::F4;
    case KEY_F5:
        return keys::F5;
    case KEY_F6:
        return keys::F6;
    case KEY_F7:
        return keys::F7;
    case KEY_F8:
        return keys::F8;
    case KEY_F9:
        return keys::F9;
    case KEY_F10:
        return keys::F10;
    case KEY_F11:
        return keys::F11;
    case KEY_F12:
        return keys::F12;
    // navigation & locks
    case KEY_ESC:
        return keys::ESCAPE;
    case KEY_INSERT:
        return keys::INSERT;
    case KEY_DELETE:
        return keys::DELETE;
    case KEY_RIGHT:
        return keys::RIGHT;
    case KEY_LEFT:
        return keys::LEFT;
    case KEY_DOWN:
        return keys::DOWN;
    case KEY_UP:
        return keys::UP;
    case KEY_PAGEUP:
        return keys::PAGE_UP;
    case KEY_PAGEDOWN:
        return keys::PAGE_DOWN;
    case KEY_HOME:
        return keys::HOME;
    case KEY_END:
        return keys::END;
    case KEY_SCROLLLOCK:
        return keys::SCROLL_LOCK;
    case KEY_NUMLOCK:
        return keys::NUM_LOCK;
    case KEY_SYSRQ:
        return keys::PRINT_SCREEN;
    case KEY_PAUSE:
        return keys::PAUSE;
    // right modifiers
    case KEY_RIGHTCTRL:
        return keys::RIGHT_CONTROL;
    case KEY_RIGHTALT:
        return keys::RIGHT_ALT;
    case KEY_LEFTMETA:
        return keys::LEFT_SUPER;
    case KEY_RIGHTMETA:
        return keys::RIGHT_SUPER;
#ifdef KEY_MENU
    case KEY_MENU:
        return keys::KB_MENU;
#endif
    // keypad
    case KEY_KP0:
        return keys::KP_0;
    case KEY_KP1:
        return keys::KP_1;
    case KEY_KP2:
        return keys::KP_2;
    case KEY_KP3:
        return keys::KP_3;
    case KEY_KP4:
        return keys::KP_4;
    case KEY_KP5:
        return keys::KP_5;
    case KEY_KP6:
        return keys::KP_6;
    case KEY_KP7:
        return keys::KP_7;
    case KEY_KP8:
        return keys::KP_8;
    case KEY_KP9:
        return keys::KP_9;
    case KEY_KPDOT:
        return keys::KP_DECIMAL;
    case KEY_KPSLASH:
        return keys::KP_DIVIDE;
    case KEY_KPASTERISK:
        return keys::KP_MULTIPLY;
    case KEY_KPMINUS:
        return keys::KP_SUBTRACT;
    case KEY_KPPLUS:
        return keys::KP_ADD;
    case KEY_KPENTER:
        return keys::KP_ENTER;
    case KEY_KPEQUAL:
        return keys::KP_EQUAL;
    default:
        break;
    }
    return keys::NONE;
}

// reverse map: keys enum to Linux key code (mirrors Windows' is_key_pressed reverse map)
static int keys_to_linux_keycode(keys key)
{
    int val = static_cast<int>(key);

    // numbers: '0'..'9' -> KEY_0..KEY_9
    if (val >= '0' && val <= '9')
        return KEY_0 + (val - '0');
    // letters — Linux key codes NOT alphabetical, must map individually
    static const struct { keys k; int code; } letter_table[] = {
        {keys::A, KEY_A}, {keys::B, KEY_B}, {keys::C, KEY_C}, {keys::D, KEY_D},
        {keys::E, KEY_E}, {keys::F, KEY_F}, {keys::G, KEY_G}, {keys::H, KEY_H},
        {keys::I, KEY_I}, {keys::J, KEY_J}, {keys::K, KEY_K}, {keys::L, KEY_L},
        {keys::M, KEY_M}, {keys::N, KEY_N}, {keys::O, KEY_O}, {keys::P, KEY_P},
        {keys::Q, KEY_Q}, {keys::R, KEY_R}, {keys::S, KEY_S}, {keys::T, KEY_T},
        {keys::U, KEY_U}, {keys::V, KEY_V}, {keys::W, KEY_W}, {keys::X, KEY_X},
        {keys::Y, KEY_Y}, {keys::Z, KEY_Z},
    };
    for (auto &e : letter_table)
        if (e.k == key)
            return e.code;

    // ASCII punctuation
    switch (val)
    {
    case '\'':
        return KEY_APOSTROPHE;
    case ',':
        return KEY_COMMA;
    case '-':
        return KEY_MINUS;
    case '.':
        return KEY_DOT;
    case '/':
        return KEY_SLASH;
    case ';':
        return KEY_SEMICOLON;
    case '=':
        return KEY_EQUAL;
    case '[':
        return KEY_LEFTBRACE;
    case '\\':
        return KEY_BACKSLASH;
    case ']':
        return KEY_RIGHTBRACE;
    case ' ':
        return KEY_SPACE;
    case '`':
        return KEY_GRAVE;
    default:
        break;
    }

    // F1–F12
    if (val >= static_cast<int>(keys::F1) && val <= static_cast<int>(keys::F12))
        return KEY_F1 + (val - static_cast<int>(keys::F1));

    // keypad 0–9
    if (val >= static_cast<int>(keys::KP_0) && val <= static_cast<int>(keys::KP_9))
        return KEY_KP0 + (val - static_cast<int>(keys::KP_0));

    // special keys — static table
    static const struct
    {
        keys k;
        int code;
    } table[] = {
        {keys::ESCAPE, KEY_ESC},
        {keys::ENTER, KEY_ENTER},
        {keys::TAB, KEY_TAB},
        {keys::BACKSPACE, KEY_BACKSPACE},
        {keys::INSERT, KEY_INSERT},
        {keys::DELETE, KEY_DELETE},
        {keys::RIGHT, KEY_RIGHT},
        {keys::LEFT, KEY_LEFT},
        {keys::DOWN, KEY_DOWN},
        {keys::UP, KEY_UP},
        {keys::PAGE_UP, KEY_PAGEUP},
        {keys::PAGE_DOWN, KEY_PAGEDOWN},
        {keys::HOME, KEY_HOME},
        {keys::END, KEY_END},
        {keys::CAPS_LOCK, KEY_CAPSLOCK},
        {keys::SCROLL_LOCK, KEY_SCROLLLOCK},
        {keys::NUM_LOCK, KEY_NUMLOCK},
        {keys::PRINT_SCREEN, KEY_SYSRQ},
        {keys::PAUSE, KEY_PAUSE},
        {keys::LEFT_SHIFT, KEY_LEFTSHIFT},
        {keys::LEFT_CONTROL, KEY_LEFTCTRL},
        {keys::LEFT_ALT, KEY_LEFTALT},
        {keys::LEFT_SUPER, KEY_LEFTMETA},
        {keys::RIGHT_SHIFT, KEY_RIGHTSHIFT},
        {keys::RIGHT_CONTROL, KEY_RIGHTCTRL},
        {keys::RIGHT_ALT, KEY_RIGHTALT},
        {keys::RIGHT_SUPER, KEY_RIGHTMETA},
#ifdef KEY_MENU
        {keys::KB_MENU, KEY_MENU},
#endif
        // keypad
        {keys::KP_DECIMAL, KEY_KPDOT},
        {keys::KP_DIVIDE, KEY_KPSLASH},
        {keys::KP_MULTIPLY, KEY_KPASTERISK},
        {keys::KP_SUBTRACT, KEY_KPMINUS},
        {keys::KP_ADD, KEY_KPPLUS},
        {keys::KP_ENTER, KEY_KPENTER},
        {keys::KP_EQUAL, KEY_KPEQUAL},
    };

    for (auto &e : table)
        if (e.k == key)
            return e.code;

    return -1; // unmapped
}

// now the `keyboards_fds[]` should be filled with `keyboard_count` events' fd
namespace keyboard
{
    keys get_key_pressed()
    {
        // query current key states via EVIOCGKEY (non‑destructive — does NOT
        // consume events, other processes still receive them)
        for (int i = 0; i < keyboard_count; i++)
        {
            unsigned char key_b[(KEY_CNT + 7) / 8];
            memset(key_b, 0, sizeof(key_b));
            if (ioctl(keyboards_fds[i], EVIOCGKEY(sizeof(key_b)), key_b) < 0)
                continue;

            for (int code = 0; code < KEY_CNT; code++)
            {
                if (key_b[code / 8] & (1 << (code % 8)))
                {
                    keys k = linux_keycode_to_keys(code);
                    if (k != keys::NONE)
                        return k;
                }
            }
        }
        return keys::NONE;
    }

    bool is_key_pressed(keys key)
    {
        if (key == keys::NONE)
            return false;

        int linux_code = keys_to_linux_keycode(key);
        if (linux_code < 0)
            return false;

        for (int i = 0; i < keyboard_count; i++)
        {
            unsigned char key_b[(KEY_CNT + 7) / 8];
            memset(key_b, 0, sizeof(key_b));
            if (ioctl(keyboards_fds[i], EVIOCGKEY(sizeof(key_b)), key_b) < 0)
                continue;

            if (key_b[linux_code / 8] & (1 << (linux_code % 8)))
                return true;
        }
        return false;
    }
}

// re-define in case mysterious bug
#define NULL ((void *)0)
