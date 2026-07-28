#include <adapter.hpp>
#include <config.hpp>
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
        fprintf(stderr, "moused: cannot open /dev/uinput (need root). "
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
    usetup.id.vendor = 0x114514;   // just.. a
    usetup.id.product = 0x1919810; // random num :)
    usetup.id.version = 0xF0C1c0;  // F?CK?
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
        fprintf(stderr, "moused: UI_DEV_CREATE failed\n");
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
    /// @brief translate cursor, same logic as win.cpp
    /// @param a the angle away from x+
    /// @param distant how many PXs will cursor translate
    void translate(angle a, int distant, unsigned int time_ms)
    {
        if (!platform_uinput_setup())
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
        if (!platform_uinput_setup())
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
        if (scale == 0.0)
            return;
        if (!platform_uinput_setup())
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

// as4 linux, we have root now so we can open /dev/...
// 4keyboard, we dont need to write /dev/uinput
//  instead reading /dev/input/event* (event* = keyboard*'s sig)

#define MAX_KEYBOARDS 16
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
        fprintf(stderr, "error occured when opening /dev/input\n");
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
    }
    closedir(input_dir);
    return true;
}

namespace keyboard
{
    keys get_key_pressed()
    {

        return keys::A;
    }

    bool is_key_pressed(keys key)
    {
        return 0;
    }
}
