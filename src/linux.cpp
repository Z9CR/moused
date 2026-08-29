#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <adapter.hpp>
#include <cmath>
#include <config.hpp>
#include <cstdio>
#include <cstring>
#include <macro_manager.hpp>
#include <utils.hpp>
#include <evdev_keys_map.hpp>

#ifdef NULL
#undef NULL  // Linux headers define NULL as `((void*)0)` (in C: __null, but
             // we're coding with CPP) — we need it for keys::NONE
#endif

#pragma region define &include
using mouse::mouse_btns;
using mouse::wheel_rotations;

constexpr double PI = 3.14159265358979323846;

enum axis { X = 0, Y = 1 };
#pragma endregion

#pragma region monitor init
// screen size — /dev/fb0, fallback 1920*1080
static int monitor_width = 1920;
static int monitor_height = 1080;

static void detect_screen() {
    int fb = open("/dev/fb0", O_RDONLY);
    if (fb >= 0) {
        fb_var_screeninfo vi;
        if (ioctl(fb, FBIOGET_VSCREENINFO, &vi) == 0) {
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

static void uinput_cleanup() {
    if (uinput_fd >= 0) {
        ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
        uinput_fd = -1;
    }
}

bool platform_uinput_setup() {
    // inited or not
    if (uinput_fd >= 0) return true;

    // val -> monitor_width&&monitor_height
    detect_screen();

    // when the `uinput` isnt stub, open it as ReadOnly
    uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    // when `open()` occurs err, it'll ret negative val
    if (uinput_fd < 0) {
        log_msg(
            "moused: cannot open /dev/uinput (need root). "
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
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_LEFT);    // LMB
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_RIGHT);   // RMB
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_MIDDLE);  // MMB
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_SIDE);    // XB1
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_EXTRA);   // XB2
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
    usetup.id.bustype = BUS_USB;  // pretend it is a USB dev
    usetup.id.vendor = 0x1145;    // just.. a
    usetup.id.product = 0x1919 + 810;   // random num :)
    usetup.id.version = 0x114 + 514;
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

    if (ioctl(uinput_fd, UI_DEV_CREATE) < 0) {
        log_msg("moused: UI_DEV_CREATE failed\n");
        close(uinput_fd);
        uinput_fd = -1;
        return false;
    }

    atexit(uinput_cleanup);
    return true;
}

// emit helpers
static inline void emit(int type, int code, int val) {
    input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = val;
    write(uinput_fd, &ev, sizeof(ev));
}

static inline void emit_sync() { emit(EV_SYN, SYN_REPORT, 0); }

// px to 0..65535  (same logic as Windows' MulDiv)
static int px2pos(int px, axis d) {
    if (d == X)
        return (int)(((long long)px * 65535 + (monitor_width - 1) / 2) /
                     (monitor_width - 1));
    else
        return (int)(((long long)px * 65535 + (monitor_height - 1) / 2) /
                     (monitor_height - 1));
}

// no pos2px 'cause the /dev/uinput is write-only

// GetCursorPos() replacement
static void get_cursor(int& x, int& y) {
    x = tracked_x;
    y = tracked_y;
}
#pragma endregion

namespace mouse {
/// @brief translate cursor by relative movement
/// @param a the angle away from x+
/// @param distant how many PXs will cursor translate
void translate(angle a, int distant, unsigned int time_ms) {
    if (!platform_uinput_setup()) return;

    // normalise angle
    if (a >= 360.0)
        while (a -= 360.0, a >= 360.0) {
        }
    if (a <= -360.0)
        while (a += 360.0, a <= -360.0) {
        }

    double rad = a * PI / 180.0;
    double total_dx = cos(rad) * distant;
    double total_dy = sin(rad) * distant;

    int frames = time_ms / smoothmv_frametime;
    if (frames < 1) frames = 1;

    double inc_x = total_dx / frames;
    double inc_y = total_dy / frames;
    double acc_x = 0.0, acc_y = 0.0;

    for (int i = 0; i < frames; ++i) {
        if (macro::g_shutdown_flag.load()) return;

        acc_x += inc_x;
        acc_y += inc_y;

        int step_x = static_cast<int>(acc_x);
        int step_y = static_cast<int>(acc_y);

        if (step_x != 0) {
            emit(EV_REL, REL_X, step_x);
            acc_x -= step_x;
        }
        if (step_y != 0) {
            emit(EV_REL, REL_Y, step_y);
            acc_y -= step_y;
        }
        if (step_x != 0 || step_y != 0) emit_sync();

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
void translate(angle a, int distant) {
    if (!platform_uinput_setup()) return;

    // normalise angle
    if (a >= 360.0)
        while (a -= 360.0, a >= 360.0) {
        }
    if (a <= -360.0)
        while (a += 360.0, a <= -360.0) {
        }

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
void move_to(unsigned int x, unsigned int y, unsigned int time_ms) {
    if (!platform_uinput_setup()) return;

    // get current position
    int start_x, start_y;
    get_cursor(start_x, start_y);

    // how many frames
    int frames = time_ms / smoothmv_frametime;
    if (frames < 1) frames = 1;

    // interpolate and send each frame
    // NOTE: bail out early on shutdown, else the UI thread's unbounded
    // join in macro::shutdown() could block on this long-running loop.
    for (int i = 1; i <= frames; ++i) {
        if (macro::g_shutdown_flag.load()) return;

        double t = static_cast<double>(i) / frames;
        int cur_x = start_x + (static_cast<int>(x) - start_x) * t;
        int cur_y = start_y + (static_cast<int>(y) - start_y) * t;

        emit(EV_ABS, ABS_X, px2pos(cur_x, X));
        emit(EV_ABS, ABS_Y, px2pos(cur_y, Y));
        emit_sync();

        usleep(smoothmv_frametime * 1000);
    }
    // in case float deviation
    if (!macro::g_shutdown_flag.load()) move_to(x, y);
}

/// @brief move the mouse to (x, y) (right = x+, down = y +)
/// @param x: how many PXs away from left boarder
/// @param y: how many PXs away from up boarder
void move_to(unsigned int x, unsigned int y) {
    if (!platform_uinput_setup()) return;

    emit(EV_ABS, ABS_X, px2pos(x, X));
    emit(EV_ABS, ABS_Y, px2pos(y, Y));
    emit_sync();

    tracked_x = x;
    tracked_y = y;
}

/// @brief click the `btn`
void click(mouse_btns btn) {
    if (!platform_uinput_setup()) return;
    press(btn);
    release(btn);
}

/// @brief press the `btn`
void press(mouse_btns btn) {
    if (!platform_uinput_setup()) return;

    int code;
    switch (btn) {
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
void release(mouse_btns btn) {
    if (!platform_uinput_setup()) return;

    int code;
    switch (btn) {
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
void wheel(wheel_rotations rotation, double scale) {
    if (!platform_uinput_setup()) return;
    if (scale == 0.0) return;

    // In Linux, one REL_WHEEL unit ~= one notch.
    // Windows WHEEL_DELTA = 120.
    int delta;
    if (scale >= 1.0 || scale <= -1.0)
        delta = static_cast<int>(scale + (scale > 0.0 ? 0.5 : -0.5));
    else if (scale > 0.0)
        delta = 1;
    else
        delta = -1;

    if (rotation == WD) delta = -delta;

    emit(EV_REL, REL_WHEEL, delta);
    emit_sync();
}
}  // namespace mouse

// as4 linux, we have root now so we can open /dev/...
// 4keyboard, we dont need to write /dev/uinput
//  instead reading /dev/input/event* (event* = keyboard*'s sig)

constexpr int MAX_KEYBOARDS = 16;
static int keyboards_fds[MAX_KEYBOARDS];
static int keyboard_count = 0;

static bool is_keyboard(int fd) {
    unsigned long evbits;
    // Get the bitmask of supported event types
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits) < 0) {
        return false;  // can't read — not a keyboard
    }

    // Check if EV_KEY (value 1) is supported
    if (evbits & (1 << EV_KEY)) {
        return true;  // Likely a keyboard or keypad
    }
    return false;
}

bool platform_keyboard_capture_setup() {
    // open dir
    DIR* input_dir = opendir("/dev/input/");
    if (!input_dir)  // when err
    {
        log_msg("error occured when opening /dev/input\n");
        return false;
    }

    dirent* entry{};
    // travel all the `input_dir`
    while ((entry = readdir(input_dir)) != nullptr) {
        // fliter event*
        if (strncmp(entry->d_name, "event", 5) != 0) continue;

        // get path — build safely with snprintf
        char fullpath[256];
        snprintf(fullpath, sizeof(fullpath), "/dev/input/%s", entry->d_name);
        // try to open
        int tmp_fd = open(fullpath, O_RDONLY);
        if (tmp_fd == -1) continue;

        if (is_keyboard(tmp_fd) && keyboard_count < MAX_KEYBOARDS) {
            keyboards_fds[keyboard_count++] = tmp_fd;
        } else {
            close(tmp_fd);  // not a keyboard or limit reached — release fd
        }
    }
    closedir(input_dir);

    if (keyboard_count == 0) {
        log_msg("moused: no keyboard devices found in /dev/input/\n");
        return false;
    }
    return true;
}

using keys = keyboard::keys;

// map Linux input key codes to keys enum (mirrors Windows' vk2keys)
static keys linux_keycode_to_keys(int code) {
    switch (code) {
#define EVDEV_ITEM(kc, ks) case kc: return ks;
        EVDEV_KEYS_LIST(EVDEV_ITEM)
#undef EVDEV_ITEM
        default:
            break;
    }
    return keys::NONE;
}

// reverse map: keys enum to Linux key code (mirrors Windows' is_key_pressed
// reverse map)
static int keys_to_linux_keycode(keys key) {
    static const struct {
        keys k;
        int code;
    } table[] = {
#define EVDEV_ITEM(kc, ks) {ks, kc},
        EVDEV_KEYS_LIST(EVDEV_ITEM)
#undef EVDEV_ITEM
    };
    for (auto& e : table)
        if (e.k == key) return e.code;
    return -1;  // unmapped
}

// now the `keyboards_fds[]` should be filled with `keyboard_count` events' fd
namespace keyboard {
keys get_key_pressed() {
    // query current key states via EVIOCGKEY (non‑destructive — does NOT
    // consume events, other processes still receive them)
    for (int i = 0; i < keyboard_count; i++) {
        unsigned char key_b[(KEY_CNT + 7) / 8];
        memset(key_b, 0, sizeof(key_b));
        if (ioctl(keyboards_fds[i], EVIOCGKEY(sizeof(key_b)), key_b) < 0)
            continue;

        for (int code = 0; code < KEY_CNT; code++) {
            if (key_b[code / 8] & (1 << (code % 8))) {
                keys k = linux_keycode_to_keys(code);
                if (k != keys::NONE) return k;
            }
        }
    }
    return keys::NONE;
}

bool is_key_pressed(keys key) {
    if (key == keys::NONE) return false;

    int linux_code = keys_to_linux_keycode(key);
    if (linux_code < 0) return false;

    for (int i = 0; i < keyboard_count; i++) {
        unsigned char key_b[(KEY_CNT + 7) / 8];
        memset(key_b, 0, sizeof(key_b));
        if (ioctl(keyboards_fds[i], EVIOCGKEY(sizeof(key_b)), key_b) < 0)
            continue;

        if (key_b[linux_code / 8] & (1 << (linux_code % 8))) return true;
    }
    return false;
}
}  // namespace keyboard

// re-define in case mysterious bug
#define NULL ((void*)0)
