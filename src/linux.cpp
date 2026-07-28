#include <adapter.hpp>
#include <config.hpp>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/uinput.h>

using mouse::mouse_btns;
using mouse::wheel_rotations;

constexpr double PI = 3.14159265358979323846;

enum axis
{
    X = 0,
    Y = 1
};

// screen size — /dev/fb0, fallback 1920*1080
static int monitor_width = 1920;
static int monitor_height = 1080;

static void detect_screen()
{
    int fb = open("/dev/fb0", O_RDONLY);
    if (fb >= 0)
    {
        struct fb_var_screeninfo vi;
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

// uinput singleton, root is required to open /dev/uinput
static int uifd = -1;

static void ui_cleanup()
{
    if (uifd >= 0)
    {
        ioctl(uifd, UI_DEV_DESTROY);
        close(uifd);
        uifd = -1;
    }
}

static int ui_init()
{
    if (uifd >= 0)
        return 0;

    detect_screen();

    uifd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uifd < 0)
    {
        fprintf(stderr, "moused: cannot open /dev/uinput (need root). "
                        "Run with sudo or as root.\n");
        return -1;
    }

    // event types
    ioctl(uifd, UI_SET_EVBIT, EV_KEY);
    ioctl(uifd, UI_SET_EVBIT, EV_REL);
    ioctl(uifd, UI_SET_EVBIT, EV_ABS);
    ioctl(uifd, UI_SET_EVBIT, EV_SYN);

    // buttons
    ioctl(uifd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(uifd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(uifd, UI_SET_KEYBIT, BTN_MIDDLE);
    ioctl(uifd, UI_SET_KEYBIT, BTN_SIDE);
    ioctl(uifd, UI_SET_KEYBIT, BTN_EXTRA);

    // relative axes
    ioctl(uifd, UI_SET_RELBIT, REL_X);
    ioctl(uifd, UI_SET_RELBIT, REL_Y);
    ioctl(uifd, UI_SET_RELBIT, REL_WHEEL);

    // absolute axes (0 … 65535, same as Windows)
    ioctl(uifd, UI_SET_ABSBIT, ABS_X);
    ioctl(uifd, UI_SET_ABSBIT, ABS_Y);

    struct uinput_setup usetup{};
    memset(&usetup, 0, sizeof(usetup));
    snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "moused");
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234;
    usetup.id.product = 0x5678;
    usetup.id.version = 1;
    ioctl(uifd, UI_DEV_SETUP, &usetup);

    struct uinput_abs_setup abs{};
    memset(&abs, 0, sizeof(abs));

    abs.code = ABS_X;
    abs.absinfo.minimum = 0;
    abs.absinfo.maximum = 65535;
    abs.absinfo.resolution = 1;
    ioctl(uifd, UI_ABS_SETUP, &abs);

    abs.code = ABS_Y;
    abs.absinfo.minimum = 0;
    abs.absinfo.maximum = 65535;
    abs.absinfo.resolution = 1;
    ioctl(uifd, UI_ABS_SETUP, &abs);

    if (ioctl(uifd, UI_DEV_CREATE) < 0)
    {
        fprintf(stderr, "moused: UI_DEV_CREATE failed\n");
        close(uifd);
        uifd = -1;
        return -1;
    }

    atexit(ui_cleanup);
    return 0;
}

int platform_uinput_setup()
{
    return ui_init();
}

// emit helpers
static inline void emit(int type, int code, int val)
{
    struct input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = val;
    write(uifd, &ev, sizeof(ev));
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

static int pos2px(int pos, axis d)
{
    if (d == X)
        return (int)(((long long)(monitor_width - 1) * pos + 65535 / 2) / 65535);
    else
        return (int)(((long long)(monitor_height - 1) * pos + 65535 / 2) / 65535);
}

// GetCursorPos() replacement
static void get_cursor(int &x, int &y)
{
    x = tracked_x;
    y = tracked_y;
}

namespace mouse
{
    /// @brief translate cursor, same logic as win.cpp
    /// @param a the angle away from x+
    /// @param distant how many PXs will cursor translate
    void translate(angle a, int distant, unsigned int time_ms)
    {
        if (ui_init() < 0)
            return;

        // GetCursorPos
        int cur_x, cur_y;
        get_cursor(cur_x, cur_y);

        // normalise angle
        if (a >= 360.0)
            while (a -= 360.0, a >= 360.0)
            {
            }
        if (a <= -360.0)
            while (a += 360.0, a <= -360.0)
            {
            }

        double rad = a * PI / 180.0;
        int dstx = cur_x + static_cast<int>(cos(rad) * distant);
        int dsty = cur_y + static_cast<int>(sin(rad) * distant);
        move_to(dstx, dsty, time_ms);
    }

    /// @brief translate cursor
    /// @param a the angle away from x+
    /// @param distant how many PXs will cursor translate
    void translate(angle a, int distant)
    {
        if (ui_init() < 0)
            return;

        int cur_x, cur_y;
        get_cursor(cur_x, cur_y);

        if (a >= 360.0)
            while (a -= 360.0, a >= 360.0)
            {
            }
        if (a <= -360.0)
            while (a += 360.0, a <= -360.0)
            {
            }

        double rad = a * PI / 180.0;
        int dstx = cur_x + static_cast<int>(cos(rad) * distant);
        int dsty = cur_y + static_cast<int>(sin(rad) * distant);
        move_to(dstx, dsty);
    }

    /// @brief move the mouse to (x, y) (right = x+, down = y +)
    /// @param x: how many PXs away from left boarder
    /// @param y: how many PXs away from up boarder
    /// @param time_ms: time duration for the smooth movement, in ms
    void move_to(unsigned int x, unsigned int y, unsigned int time_ms)
    {
        if (ui_init() < 0)
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
        if (ui_init() < 0)
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
        if (ui_init() < 0)
            return;
        press(btn);
        release(btn);
    }

    /// @brief press the `btn`
    void press(mouse_btns btn)
    {
        if (ui_init() < 0)
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
        if (ui_init() < 0)
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
        if (scale == 0.0)
            return;
        if (ui_init() < 0)
            return;

        // In Linux, one REL_WHEEL unit ~= one notch.
        // Windows WHEEL_DELTA = 120.
        int delta;
        if (scale >= 1.0 || scale <= -1.0)
            delta = static_cast<int>(scale);
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

namespace keyboard {
    keys get_key_pressed() {

        return keys::A;
    }

    bool is_key_pressed(keys key) {
        return 0;
    }
}