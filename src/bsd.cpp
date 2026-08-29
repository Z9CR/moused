// BSD backend for moused (FreeBSD / OpenBSD / NetBSD / DragonFly).
//
// BSDs have no single Linux-style /dev/uinput, so the backend uses each
// family's *native* input-injection interface. All inject *below* the
// display server — the server reads the very same event stream a real mouse
// would produce — which makes them work identically under X11 and Wayland:
//
//   FreeBSD:
//     - mouse injection:  /dev/uinput. FreeBSD's input subsystem ships the
//       Linux-compatible uinput device (the GENERIC kernel includes
//       `device uinput`); the ioctl interface is UI_SET_EVBIT / UI_SET_KEYBIT
//       / UI_SET_RELBIT / UI_DEV_SETUP / UI_DEV_CREATE (see
//       sys/dev/evdev/uinput.{c,h}).
//     - keyboard capture: /dev/input/event* + EVIOCGKEY. FreeBSD's evdev is
//       a Linux-compatible implementation (see sys/dev/evdev/cdev.c), so
//       this is byte-for-byte the same code as the Linux backend.
//
//   DragonFly:
//     - mouse injection:  DragonFly has no uinput (its evdev was synced from
//       FreeBSD explicitly "w/o uinput"), but its evdev character devices
//       inject whatever input_event structs are written to the device node
//       (sys/dev/misc/evdev/cdev.c evdev_write() -> evdev_inject_event()).
//       We pick a real REL mouse device and write events into its node, so
//       the display server sees them in the very event stream it reads.
//     - keyboard capture: same evdev EVIOCGKEY code as FreeBSD.
//
//   OpenBSD / NetBSD:
//     - mouse injection:  /dev/wsmouse (the wscons *mouse mux*, a.k.a.
//       /dev/wsmux0) via the WSMUXIO_INJECTEVENT ioctl. The window system
//       (X's xf86-input-ws driver, or a Wayland compositor) reads this mux,
//       so injected events reach it no matter which display server runs.
//     - keyboard capture: /dev/wskbd*. wscons has no key-state query ioctl
//       and a wskbd device can have only ONE reader (the window system).
//       We try to open it read-only; when the window system owns the
//       keyboard the open() fails with EBUSY and hotkeys are simply
//       unavailable (the app still runs; macros just can't be triggered).
//
// Movement is relative (REL_X/REL_Y or DELTA_X/DELTA_Y) everywhere, so the
// cursor position is tracked locally. Same caveat as the Linux backend's
// /dev/uinput tracking: if the user moves the mouse externally our internal
// position goes stale until the next absolute move_to() recalibrates it.

#include <adapter.hpp>
#include <config.hpp>
#include <macro_manager.hpp>
#include <utils.hpp>
#include <evdev_keys_map.hpp>
#include <evdev_bsd.hpp>  // self-contained evdev/uinput constants (FreeBSD/DragonFly)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

using mouse::mouse_btns;
using mouse::wheel_rotations;

constexpr double PI = 3.14159265358979323846;


// guard against a zero/negative frame time from a broken config
static int frame_time_ms() {
    return smoothmv_frametime > 0 ? smoothmv_frametime : 4;
}

// FreeBSD — uinput virtual mouse + evdev keyboard capture
// (the evdev/uinput constants come from evdev_bsd.hpp — FreeBSD's base
// system does not install <dev/evdev/*> into /usr/include for userland)
#if defined(__FreeBSD__)
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

// uinput singleton; root (or a permissive /dev/uinput mode) is required
int g_mouse_fd = -1;

// local cursor tracking (see the file header comment)
std::mutex g_track_mutex;
int tracked_x = 0;
int tracked_y = 0;

void uinput_cleanup() {
    if (g_mouse_fd >= 0) {
        ioctl(g_mouse_fd, UI_DEV_DESTROY);
        close(g_mouse_fd);
        g_mouse_fd = -1;
    }
}

void bsd_emit(int type, int code, int val) {
    if (g_mouse_fd < 0) return;
    input_event ev{};
    ev.type = static_cast<uint16_t>(type);
    ev.code = static_cast<uint16_t>(code);
    ev.value = val;
    write(g_mouse_fd, &ev, sizeof(ev));
}

void bsd_emit_sync() { bsd_emit(EV_SYN, SYN_REPORT, 0); }

// dx positive = right, dy positive = down (screen coordinates)
void bsd_motion(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    if (dx != 0) bsd_emit(EV_REL, REL_X, dx);
    if (dy != 0) bsd_emit(EV_REL, REL_Y, dy);
    bsd_emit_sync();
}

// dz positive = wheel up (Linux REL_WHEEL convention)
void bsd_wheel(int dz) {
    if (dz == 0) return;
    bsd_emit(EV_REL, REL_WHEEL, dz);
    bsd_emit_sync();
}

// button: 0=left, 1=middle, 2=right, 3=back(XB1), 4=forward(XB2)
void bsd_button(int button, bool down) {
    static const int btns[] = {BTN_LEFT, BTN_MIDDLE, BTN_RIGHT, BTN_SIDE,
                               BTN_EXTRA};
    if (button < 0 || button > 4) return;
    bsd_emit(EV_KEY, btns[button], down ? 1 : 0);
    bsd_emit_sync();
}

}  // namespace

bool platform_uinput_setup() {
    // inited or not
    if (g_mouse_fd >= 0) return true;

    g_mouse_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (g_mouse_fd < 0) {
        log_msg("moused: cannot open /dev/uinput (need root; the GENERIC "
                "FreeBSD kernel ships `device uinput`, so on a custom "
                "kernel make sure it is present). Run with sudo or as "
                "root.\n");
        return false;
    }

    // event types the virtual mouse can emit
    ioctl(g_mouse_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(g_mouse_fd, UI_SET_EVBIT, EV_REL);
    ioctl(g_mouse_fd, UI_SET_EVBIT, EV_SYN);

    // mouse buttons (LMB / RMB / MMB + two side buttons)
    ioctl(g_mouse_fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(g_mouse_fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(g_mouse_fd, UI_SET_KEYBIT, BTN_MIDDLE);
    ioctl(g_mouse_fd, UI_SET_KEYBIT, BTN_SIDE);
    ioctl(g_mouse_fd, UI_SET_KEYBIT, BTN_EXTRA);

    // relative axes (pointer movement + wheel)
    ioctl(g_mouse_fd, UI_SET_RELBIT, REL_X);
    ioctl(g_mouse_fd, UI_SET_RELBIT, REL_Y);
    ioctl(g_mouse_fd, UI_SET_RELBIT, REL_WHEEL);

    // create a virtual mouse named "moused" (same fake identity as the
    // Linux backend)
    uinput_setup usetup{};
    memset(&usetup, 0, sizeof(usetup));
    snprintf(usetup.name, sizeof(usetup.name), "moused");
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1145;
    usetup.id.product = 0x1919 + 810;
    usetup.id.version = 0x114 + 514;
    ioctl(g_mouse_fd, UI_DEV_SETUP, &usetup);

    if (ioctl(g_mouse_fd, UI_DEV_CREATE) < 0) {
        log_msg("moused: UI_DEV_CREATE failed\n");
        close(g_mouse_fd);
        g_mouse_fd = -1;
        return false;
    }

    atexit(uinput_cleanup);
    return true;
}

// ---- FreeBSD keyboard capture (evdev, Linux-compatible) ----
constexpr int MAX_KEYBOARDS = 16;
static int keyboards_fds[MAX_KEYBOARDS];
static int keyboard_count = 0;

static bool is_keyboard(int fd) {
    unsigned long evbits;
    // Get the bitmask of supported event types
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits) < 0) return false;
    // Check if EV_KEY (value 1) is supported
    return (evbits & (1 << EV_KEY)) != 0;
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
        log_msg("moused: no keyboard devices found in /dev/input/ "
                "(is the evdev module loaded on FreeBSD?)\n");
        return false;
    }
    return true;
}

using keys = keyboard::keys;

// map Linux/evdev input key codes to keys enum (mirrors Windows' vk2keys)
static keys bsd_keycode_to_keys(int code) {
    switch (code) {
#define EVDEV_ITEM(kc, ks) case kc: return ks;
        EVDEV_KEYS_LIST(EVDEV_ITEM)
#undef EVDEV_ITEM
        default:
            break;
    }
    return keys::NONE;
}

// reverse map: keys enum to evdev key code (mirrors Windows' is_key_pressed
// reverse map)
static int keys_to_bsd_keycode(keys key) {
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

namespace keyboard {
keys get_key_pressed() {
    // query current key states via EVIOCGKEY (non-destructive — does NOT
    // consume events, other processes still receive them)
    for (int i = 0; i < keyboard_count; i++) {
        unsigned char key_b[(KEY_CNT + 7) / 8];
        memset(key_b, 0, sizeof(key_b));
        if (ioctl(keyboards_fds[i], EVIOCGKEY(sizeof(key_b)), key_b) < 0)
            continue;

        for (int code = 0; code < KEY_CNT; code++) {
            if (key_b[code / 8] & (1 << (code % 8))) {
                keys k = bsd_keycode_to_keys(code);
                if (k != keys::NONE) return k;
            }
        }
    }
    return keys::NONE;
}

bool is_key_pressed(keys key) {
    if (key == keys::NONE) return false;

    int code = keys_to_bsd_keycode(key);
    if (code < 0) return false;

    for (int i = 0; i < keyboard_count; i++) {
        unsigned char key_b[(KEY_CNT + 7) / 8];
        memset(key_b, 0, sizeof(key_b));
        if (ioctl(keyboards_fds[i], EVIOCGKEY(sizeof(key_b)), key_b) < 0)
            continue;

        if (key_b[code / 8] & (1 << (code % 8))) return true;
    }
    return false;
}
}  // namespace keyboard

// DragonFly — evdev write-injection + evdev keyboard capture
#elif defined(__DragonFly__)

#include <dirent.h>
#include <fcntl.h>
#include <sys/ioccom.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

// DragonFly's evdev was synced from FreeBSD explicitly WITHOUT uinput, and
// the evdev headers are not installed for userland.  The ioctl numbers,
// event codes and KEY_* values used below come from evdev_bsd.hpp (values
// identical to the FreeBSD/Linux ones).

namespace {

// the evdev mouse device we inject into; opened WRITE-ONLY
int g_mouse_fd = -1;

// local cursor tracking (see the file header comment)
std::mutex g_track_mutex;
int tracked_x = 0;
int tracked_y = 0;

void bsd_emit(int type, int code, int val) {
    if (g_mouse_fd < 0) return;
    input_event ev{};
    ev.type = static_cast<uint16_t>(type);
    ev.code = static_cast<uint16_t>(code);
    ev.value = val;
    write(g_mouse_fd, &ev, sizeof(ev));
}

void bsd_emit_sync() { bsd_emit(EV_SYN, SYN_REPORT, 0); }

// dx positive = right, dy positive = down (screen coordinates)
void bsd_motion(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    if (dx != 0) bsd_emit(EV_REL, REL_X, dx);
    if (dy != 0) bsd_emit(EV_REL, REL_Y, dy);
    bsd_emit_sync();
}

// dz positive = wheel up (Linux REL_WHEEL convention)
void bsd_wheel(int dz) {
    if (dz == 0) return;
    bsd_emit(EV_REL, REL_WHEEL, dz);
    bsd_emit_sync();
}

// button: 0=left, 1=middle, 2=right, 3=back(XB1), 4=forward(XB2)
void bsd_button(int button, bool down) {
    static const int btns[] = {BTN_LEFT, BTN_MIDDLE, BTN_RIGHT, BTN_SIDE,
                               BTN_EXTRA};
    if (button < 0 || button > 4) return;
    bsd_emit(EV_KEY, btns[button], down ? 1 : 0);
    bsd_emit_sync();
}

}  // namespace

bool platform_uinput_setup() {
    if (g_mouse_fd >= 0) return true;

    // No /dev/uinput: DragonFly injects whatever input_event structs are
    // written to a /dev/input/event* node.  Pick a real REL mouse device
    // (preferring one without absolute axes, i.e. not a touchpad) and write
    // into its event stream — the display server reads exactly that stream.
    // NOTE: with several pointers the chosen device may not be the one the
    // display server reads; the common single-mouse desktop works directly.
    int best_fd = -1;
    bool best_has_abs = false;
    char best_path[256] = "";

    DIR* input_dir = opendir("/dev/input/");
    if (!input_dir) {
        log_msg("moused: cannot open /dev/input on DragonFly\n");
        return false;
    }

    dirent* entry{};
    while ((entry = readdir(input_dir)) != nullptr) {
        // filter event*
        if (strncmp(entry->d_name, "event", 5) != 0) continue;

        char fullpath[256];
        snprintf(fullpath, sizeof(fullpath), "/dev/input/%s", entry->d_name);
        int tmp_fd = open(fullpath, O_RDONLY | O_NONBLOCK);
        if (tmp_fd < 0) continue;

        // must be a REL mouse: EV_REL + EV_KEY + REL_X/REL_Y + BTN_LEFT
        unsigned long evbits = 0, relbits = 0;
        unsigned char keybits[(KEY_CNT + 7) / 8] = {0};
        bool ok =
            ioctl(tmp_fd, EVIOCGBIT(0, sizeof(evbits)), &evbits) == 0 &&
            ioctl(tmp_fd, EVIOCGBIT(EV_REL, sizeof(relbits)),
                  &relbits) == 0 &&
            ioctl(tmp_fd, EVIOCGBIT(EV_KEY, sizeof(keybits)),
                  &keybits) == 0;
        bool has_left =
            (keybits[BTN_LEFT / 8] & (1 << (BTN_LEFT % 8))) != 0;
        if (!ok || !(evbits & (1UL << EV_REL)) ||
            !(evbits & (1UL << EV_KEY)) ||
            !(relbits & (1UL << REL_X)) || !(relbits & (1UL << REL_Y)) ||
            !has_left) {
            close(tmp_fd);
            continue;
        }

        bool has_abs = (evbits & (1UL << EV_ABS)) != 0;
        if (best_fd < 0 || (has_abs && !best_has_abs)) {
            // keep the first candidate, but swap an absolute device (e.g. a
            // touchpad) for a plain REL mouse if one appears later
            if (best_fd >= 0) close(best_fd);
            best_fd = tmp_fd;
            best_has_abs = has_abs;
            snprintf(best_path, sizeof(best_path), "%s", fullpath);
        } else {
            close(tmp_fd);
        }
    }
    closedir(input_dir);

    if (best_fd < 0) {
        log_msg("moused: no evdev REL mouse found under /dev/input on "
                "DragonFly (is the evdev module loaded?)\n");
        return false;
    }

    // close the O_RDONLY probe and reopen WRITE-ONLY for injection
    close(best_fd);
    g_mouse_fd = open(best_path, O_WRONLY | O_NONBLOCK);
    if (g_mouse_fd < 0) {
        log_msg("moused: cannot open %s for writing (run with doas/sudo)\n",
                best_path);
        g_mouse_fd = -1;
        return false;
    }
    return true;
}

// ---- DragonFly keyboard capture (evdev, Linux-compatible ioctls) ----
constexpr int MAX_KEYBOARDS = 16;
static int keyboards_fds[MAX_KEYBOARDS];
static int keyboard_count = 0;

static bool is_keyboard(int fd) {
    unsigned long evbits;
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits) < 0) return false;
    return (evbits & (1UL << EV_KEY)) != 0;
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
        // filter event*
        if (strncmp(entry->d_name, "event", 5) != 0) continue;

        char fullpath[256];
        snprintf(fullpath, sizeof(fullpath), "/dev/input/%s", entry->d_name);
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
        log_msg("moused: no keyboard devices found in /dev/input/ "
                "(is the evdev module loaded on DragonFly?)\n");
        return false;
    }
    return true;
}

using keys = keyboard::keys;

// map evdev input key codes to keys enum (mirrors the FreeBSD mapping)
static keys bsd_keycode_to_keys(int code) {
    switch (code) {
#define EVDEV_ITEM(kc, ks) case kc: return ks;
        EVDEV_KEYS_LIST(EVDEV_ITEM)
#undef EVDEV_ITEM
        default:
            break;
    }
    return keys::NONE;
}

// reverse map: keys enum to evdev key code (mirrors the FreeBSD mapping)
static int keys_to_bsd_keycode(keys key) {
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

namespace keyboard {
keys get_key_pressed() {
    // query current key states via EVIOCGKEY (non-destructive — does NOT
    // consume events, other processes still receive them)
    for (int i = 0; i < keyboard_count; i++) {
        unsigned char key_b[(KEY_CNT + 7) / 8];
        memset(key_b, 0, sizeof(key_b));
        if (ioctl(keyboards_fds[i], EVIOCGKEY(sizeof(key_b)), key_b) < 0)
            continue;

        for (int code = 0; code < KEY_CNT; code++) {
            if (key_b[code / 8] & (1 << (code % 8))) {
                keys k = bsd_keycode_to_keys(code);
                if (k != keys::NONE) return k;
            }
        }
    }
    return keys::NONE;
}

bool is_key_pressed(keys key) {
    if (key == keys::NONE) return false;

    int code = keys_to_bsd_keycode(key);
    if (code < 0) return false;

    for (int i = 0; i < keyboard_count; i++) {
        unsigned char key_b[(KEY_CNT + 7) / 8];
        memset(key_b, 0, sizeof(key_b));
        if (ioctl(keyboards_fds[i], EVIOCGKEY(sizeof(key_b)), key_b) < 0)
            continue;

        if (key_b[code / 8] & (1 << (code % 8))) return true;
    }
    return false;
}
}  // namespace keyboard

// ========================================================================
// OpenBSD / NetBSD — wscons mux injection + wskbd capture
// ========================================================================
#else  // OpenBSD / NetBSD

#include <dev/wscons/wsconsio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

using keys = keyboard::keys;

namespace {

// keys enum values run from 0 to 348 (KB_MENU is the largest, see
// include/enums_list.hpp)
constexpr int MOUSED_KEYS_N = 349;
// mouse mux device (a.k.a. /dev/wsmux0); opened WRITE-ONLY so it works even
// while the window system is reading the mux
int g_mouse_fd = -1;

std::mutex g_track_mutex;
int tracked_x = 0;
int tracked_y = 0;

void wscons_inject(int type, int value) {
    if (g_mouse_fd < 0) return;
    wscons_event ev{};
    ev.type = static_cast<unsigned int>(type);
    ev.value = value;
    ioctl(g_mouse_fd, WSMUXIO_INJECTEVENT, &ev);
}

// dx positive = right, dy positive = down (screen coordinates).
// wscons DELTA_Y is inverted relative to screen coords — the X ws driver
// does dy = -event->value — hence the negation.
void bsd_motion(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    if (dx != 0) wscons_inject(WSCONS_EVENT_MOUSE_DELTA_X, dx);
    if (dy != 0) wscons_inject(WSCONS_EVENT_MOUSE_DELTA_Y, -dy);
}

// dz positive = wheel up (Linux REL_WHEEL convention). The X ws driver
// passes DELTA_Z through unchanged (hw->dz = event->value) and maps
// positive -> button 4 (wheel up) via ZAxisMapping "4 5", so inject the
// value as-is (no negation — a negated value scrolls the wrong way).
void bsd_wheel(int dz) {
    if (dz == 0) return;
    wscons_inject(WSCONS_EVENT_MOUSE_DELTA_Z, dz);
}

// button 0..4: 0=left, 1=middle, 2=right, 3=back(XB1), 4=forward(XB2).
// wscons event value N == X button N+1 (see the X ws driver:
// buttons |= 1 << event->value), which matches our 0..4 numbering.
void bsd_button(int button, bool down) {
    if (button < 0 || button > 4) return;
    wscons_inject(down ? WSCONS_EVENT_MOUSE_DOWN : WSCONS_EVENT_MOUSE_UP,
                  button);
}

// ---- wskbd keyboard capture (best-effort) ----
constexpr int WSKBD_MAX = 8;
int wskbd_fds[WSKBD_MAX];
int wskbd_count = 0;

// wscons keycode -> keys enum value (-1 unmapped); wscons keycodes run up to
// KS_NUMKEYCODES (0x1000)
int wskbd_keycode_to_keys[0x1000];
// pressed state, indexed by keys enum value
std::vector<char> pressed_keys;
std::mutex pressed_mutex;
std::jthread wskbd_thread;


// map a wscons keysym (KS_*, see sys/dev/wscons/wsksymdef.h) to the keys
// enum
keys wscons_ksym_to_keys(uint16_t ks) {
    // printable ASCII — wscons keysyms for letters/digits/punct are ASCII
    if (ks >= 0x20 && ks <= 0x7e) {
        if (ks >= 'a' && ks <= 'z') ks -= 32;  // keys enum uses uppercase
        return static_cast<keys>(ks);
    }
    switch (ks) {
        case 0x08: return keys::BACKSPACE;     // KS_BackSpace
        case 0x09: return keys::TAB;           // KS_Tab
        case 0x0d: return keys::ENTER;         // KS_Return
        case 0x1b: return keys::ESCAPE;        // KS_Escape
        case 0x7f: return keys::DELETE;        // KS_Delete
        case 0xf101: return keys::LEFT_SHIFT;   // KS_Shift_L
        case 0xf102: return keys::RIGHT_SHIFT;  // KS_Shift_R
        case 0xf103: return keys::LEFT_CONTROL; // KS_Control_L
        case 0xf104: return keys::RIGHT_CONTROL;// KS_Control_R
        case 0xf105: return keys::CAPS_LOCK;    // KS_Caps_Lock
        case 0xf106: return keys::CAPS_LOCK;    // KS_Shift_Lock
        case 0xf107: return keys::LEFT_ALT;     // KS_Alt_L
        case 0xf108: return keys::RIGHT_ALT;    // KS_Alt_R
        case 0xf10b: return keys::NUM_LOCK;     // KS_Num_Lock
        case 0xf110: return keys::LEFT_SUPER;   // KS_Meta_L
        case 0xf111: return keys::RIGHT_SUPER;  // KS_Meta_R
        case 0xf20d: return keys::KP_ENTER;     // KS_KP_Enter
        case 0xf22a: return keys::KP_MULTIPLY;  // KS_KP_Multiply
        case 0xf22b: return keys::KP_ADD;       // KS_KP_Add
        case 0xf22d: return keys::KP_SUBTRACT;  // KS_KP_Subtract
        case 0xf22e: return keys::KP_DECIMAL;   // KS_KP_Decimal
        case 0xf22f: return keys::KP_DIVIDE;    // KS_KP_Divide
        case 0xf23d: return keys::KP_EQUAL;     // KS_KP_Equal
        case 0xf381: return keys::HOME;         // KS_Home
        case 0xf382: return keys::PAGE_UP;      // KS_Prior
        case 0xf383: return keys::PAGE_DOWN;    // KS_Next
        case 0xf384: return keys::UP;           // KS_Up
        case 0xf385: return keys::DOWN;         // KS_Down
        case 0xf386: return keys::LEFT;         // KS_Left
        case 0xf387: return keys::RIGHT;        // KS_Right
        case 0xf388: return keys::END;          // KS_End
        case 0xf389: return keys::INSERT;       // KS_Insert
        case 0xf3c0: return keys::KB_MENU;      // KS_Menu
        case 0xf3c1: return keys::PAUSE;        // KS_Pause
        case 0xf3c2: return keys::PRINT_SCREEN; // KS_Print_Screen
        default:
            break;
    }
    // keypad 0-9 (KS_KP_0..KS_KP_9)
    if (ks >= 0xf230 && ks <= 0xf239)
        return static_cast<keys>(static_cast<int>(keys::KP_0) +
                                 (ks - 0xf230));
    // F1-F12 — wscons defines both KS_f1..KS_f12 (0xf300..) and
    // KS_F1..KS_F12 (0xf340..); handle both forms
    if (ks >= 0xf300 && ks <= 0xf30b)
        return static_cast<keys>(static_cast<int>(keys::F1) + (ks - 0xf300));
    if (ks >= 0xf340 && ks <= 0xf34b)
        return static_cast<keys>(static_cast<int>(keys::F1) + (ks - 0xf340));
    return keys::NONE;
}


// background reader: maintains the pressed-state table from wskbd events
void wskbd_reader(std::stop_token st) {
    while (!st.stop_requested()) {
        bool got = false;
        for (int i = 0; i < wskbd_count; ++i) {
            for (;;) {
                wscons_event ev{};
                ssize_t n = read(wskbd_fds[i], &ev, sizeof(ev));
                if (n != (ssize_t)sizeof(ev)) break;  // EAGAIN / EOF
                got = true;

                if (ev.type == WSCONS_EVENT_ALL_KEYS_UP) {
                    std::lock_guard<std::mutex> lock(pressed_mutex);
                    std::fill(pressed_keys.begin(), pressed_keys.end(), 0);
                    continue;
                }
                if (ev.type != WSCONS_EVENT_KEY_DOWN &&
                    ev.type != WSCONS_EVENT_KEY_UP)
                    continue;

                int val = -1;
                if (ev.value >= 0 && ev.value < 0x1000)
                    val = wskbd_keycode_to_keys[ev.value];
                if (val < 0) continue;

                std::lock_guard<std::mutex> lock(pressed_mutex);
                pressed_keys[val] =
                    (ev.type == WSCONS_EVENT_KEY_DOWN) ? 1 : 0;
            }
        }
        if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
}

}  // namespace

bool platform_uinput_setup() {
    if (g_mouse_fd >= 0) return true;

    // The wscons mouse *mux* is /dev/wsmouse on OpenBSD and /dev/wsmux0 on
    // NetBSD. A write-only open never claims the event queue, so it
    // succeeds even while the window system is reading the mux.
#if defined(__NetBSD__)
    g_mouse_fd = open("/dev/wsmux0", O_WRONLY);
    if (g_mouse_fd < 0) g_mouse_fd = open("/dev/wsmouse", O_WRONLY);
#else   // OpenBSD
    g_mouse_fd = open("/dev/wsmouse", O_WRONLY);
    if (g_mouse_fd < 0) g_mouse_fd = open("/dev/wsmux0", O_WRONLY);
#endif
    if (g_mouse_fd < 0) {
        log_msg("moused: cannot open the wscons mouse mux "
                "(/dev/wsmouse or /dev/wsmux0). Is the `wsmux` pseudo-device "
                "in the kernel and do you have permission? Run with doas or "
                "sudo.\n");
        return false;
    }
    return true;
}

bool platform_keyboard_capture_setup() {
    // Try to open every /dev/wskbd*. In a desktop session the window
    // system already owns the keyboard, open() fails with EBUSY and we
    // report that hotkeys are unavailable. (Opening a *free* wskbd detaches
    // it from the console mux — acceptable: moused is a GUI app that only
    // runs while a display server is active anyway.)
    for (int i = 0; i < WSKBD_MAX && wskbd_count < WSKBD_MAX; ++i) {
        char dev[32];
        snprintf(dev, sizeof(dev), "/dev/wskbd%d", i);
        int fd = open(dev, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;  // ENXIO: no device, EBUSY: in use by server

        // fetch the keymap so keycodes (event values) can be resolved to
        // keys.  The two wscons families differ here: OpenBSD's
        // WSKBDIO_GETMAP fills an array of struct wscons_keymap
        // (group1[0] == unshifted keysym of each keycode), while NetBSD's
        // fills a flat keysym_t array instead — so the buffer type and the
        // per-keycode read differ per OS.
#if defined(__OpenBSD__)
        static wscons_keymap map_buf[0x1000];
        wskbd_map_data md{};
        md.maplen = 0x1000;
        md.map = map_buf;
        if (ioctl(fd, WSKBDIO_GETMAP, &md) < 0) {
            close(fd);
            continue;
        }
        int maplen = md.maplen;
        if (maplen > 0x1000) maplen = 0x1000;
        for (int kc = 0; kc < 0x1000; ++kc)
            wskbd_keycode_to_keys[kc] = -1;
        for (int kc = 0; kc < maplen; ++kc) {
            keys k = wscons_ksym_to_keys(map_buf[kc].group1[0]);
            wskbd_keycode_to_keys[kc] =
                (k == keys::NONE) ? -1 : static_cast<int>(k);
        }
#else   // __NetBSD__
        static keysym_t map_buf[0x1000];
        wskbd_map_data md{};
        md.maplen = 0x1000;
        md.map = map_buf;
        if (ioctl(fd, WSKBDIO_GETMAP, &md) < 0) {
            close(fd);
            continue;
        }
        int maplen = md.maplen;
        if (maplen > 0x1000) maplen = 0x1000;
        for (int kc = 0; kc < 0x1000; ++kc)
            wskbd_keycode_to_keys[kc] = -1;
        for (int kc = 0; kc < maplen; ++kc) {
            keys k = wscons_ksym_to_keys(map_buf[kc]);
            wskbd_keycode_to_keys[kc] =
                (k == keys::NONE) ? -1 : static_cast<int>(k);
        }
#endif

        wskbd_fds[wskbd_count++] = fd;
    }

    if (wskbd_count == 0) {
        log_msg("moused: warning: no wscons keyboard available for hotkey "
                "capture (the window system owns /dev/wskbd*). Macros can "
                "not be triggered by hotkeys on this system.\n");
        return false;
    }

    pressed_keys.assign(MOUSED_KEYS_N, 0);
    wskbd_thread = std::jthread(wskbd_reader);
    return true;
}

namespace keyboard {
keys get_key_pressed() {
    std::lock_guard<std::mutex> lock(pressed_mutex);
    for (int v = 1; v < MOUSED_KEYS_N; ++v)
        if (pressed_keys[v]) return static_cast<keys>(v);
    return keys::NONE;
}

bool is_key_pressed(keys key) {
    if (key == keys::NONE) return false;
    int val = static_cast<int>(key);
    if (val < 0 || val >= MOUSED_KEYS_N) return false;
    std::lock_guard<std::mutex> lock(pressed_mutex);
    return pressed_keys[val] != 0;
}
}  // namespace keyboard

#endif  // __FreeBSD__ / __DragonFly__ / wscons
namespace mouse {

/// @brief translate cursor
/// @param a the angle away from x+
/// @param distant how many PXs will cursor translate
void translate(angle a, int distant, unsigned int time_ms) {
    if (!platform_uinput_setup()) return;

    // normalise angle — same loop as the Windows backend
    if (a >= 360.0)
        while (a -= 360.0, a >= 360.0) {
        }
    if (a <= -360.0)
        while (a += 360.0, a <= -360.0) {
        }

    double rad = a * PI / 180.0;
    double total_dx = cos(rad) * distant;
    double total_dy = sin(rad) * distant;

    int frames = static_cast<int>(time_ms) / frame_time_ms();
    if (frames < 1) frames = 1;

    // sub-pixel accumulator so the per-frame integer deltas add up to the
    // requested float distance (same technique as the Linux backend)
    double inc_x = total_dx / frames;
    double inc_y = total_dy / frames;
    double acc_x = 0.0, acc_y = 0.0;

    for (int i = 0; i < frames; ++i) {
        if (macro::g_shutdown_flag.load()) return;

        acc_x += inc_x;
        acc_y += inc_y;
        int step_x = static_cast<int>(acc_x);
        int step_y = static_cast<int>(acc_y);
        if (step_x != 0 || step_y != 0) bsd_motion(step_x, step_y);
        acc_x -= step_x;
        acc_y -= step_y;

        std::this_thread::sleep_for(std::chrono::milliseconds(frame_time_ms()));
    }

    // flush the remaining subpixel remainder
    int rem_x = static_cast<int>(std::round(acc_x));
    int rem_y = static_cast<int>(std::round(acc_y));
    if (rem_x != 0 || rem_y != 0) bsd_motion(rem_x, rem_y);

    std::lock_guard<std::mutex> lock(g_track_mutex);
    tracked_x += static_cast<int>(std::round(total_dx));
    tracked_y += static_cast<int>(std::round(total_dy));
}

/// @brief translate cursor
/// @param a the angle away from x+
/// @param distant how many PXs will cursor translate
void translate(angle a, int distant) {
    if (!platform_uinput_setup()) return;

    // normalise angle — same loop as the Windows backend
    if (a >= 360.0)
        while (a -= 360.0, a >= 360.0) {
        }
    if (a <= -360.0)
        while (a += 360.0, a <= -360.0) {
        }

    double rad = a * PI / 180.0;
    // Windows truncates here (static_cast<int>), keep the same behaviour
    int dx = static_cast<int>(cos(rad) * distant);
    int dy = static_cast<int>(sin(rad) * distant);

    if (dx != 0 || dy != 0) bsd_motion(dx, dy);

    std::lock_guard<std::mutex> lock(g_track_mutex);
    tracked_x += dx;
    tracked_y += dy;
}

/// @brief move the mouse to (x, y) (right = x+, down = y +)
/// @param x: how many PXs away from left boarder
/// @param y: how many PXs away from up boarder
/// @param time_ms: time duration for the smooth movement, in ms
void move_to(unsigned int x, unsigned int y, unsigned int time_ms) {
    if (!platform_uinput_setup()) return;

    // current (tracked) position is the interpolation start
    int start_x, start_y;
    {
        std::lock_guard<std::mutex> lock(g_track_mutex);
        start_x = tracked_x;
        start_y = tracked_y;
    }

    int frames = static_cast<int>(time_ms) / frame_time_ms();
    if (frames < 1) frames = 1;

    // interpolate and emit relative deltas each frame
    // NOTE: bail out early on shutdown, else the UI thread's unbounded
    // join in macro::shutdown() could block on this long-running loop.
    for (int i = 1; i <= frames; ++i) {
        if (macro::g_shutdown_flag.load()) return;

        double t = static_cast<double>(i) / frames;
        int cur_x = start_x + (static_cast<int>(x) - start_x) * t;
        int cur_y = start_y + (static_cast<int>(y) - start_y) * t;

        int dx, dy;
        {
            std::lock_guard<std::mutex> lock(g_track_mutex);
            dx = cur_x - tracked_x;
            dy = cur_y - tracked_y;
            tracked_x = cur_x;
            tracked_y = cur_y;
        }
        if (dx != 0 || dy != 0) bsd_motion(dx, dy);

        std::this_thread::sleep_for(std::chrono::milliseconds(frame_time_ms()));
    }
    // in case float deviation
    if (!macro::g_shutdown_flag.load()) move_to(x, y);
}

/// @brief move the mouse to (x, y) (right = x+, down = y +)
/// @param x: how many PXs away from left boarder
/// @param y: how many PXs away from up boarder
void move_to(unsigned int x, unsigned int y) {
    if (!platform_uinput_setup()) return;

    std::lock_guard<std::mutex> lock(g_track_mutex);
    int dx = static_cast<int>(x) - tracked_x;
    int dy = static_cast<int>(y) - tracked_y;
    tracked_x = static_cast<int>(x);
    tracked_y = static_cast<int>(y);
    if (dx != 0 || dy != 0) bsd_motion(dx, dy);
}


/// @brief click the `btn`
void click(mouse_btns btn) {
    press(btn);
    release(btn);
}

/// @brief press the `btn`
void press(mouse_btns btn) {
    if (!platform_uinput_setup()) return;

    int b;
    switch (btn) {
        case LMB:
            b = 0;
            break;
        case MMB:
            b = 1;
            break;
        case RMB:
            b = 2;
            break;
        case XB1:
            b = 3;
            break;
        case XB2:
            b = 4;
            break;
        default:
            return;
    }
    bsd_button(b, true);
}

/// @brief release the pressed `btn`
void release(mouse_btns btn) {
    if (!platform_uinput_setup()) return;

    int b;
    switch (btn) {
        case LMB:
            b = 0;
            break;
        case MMB:
            b = 1;
            break;
        case RMB:
            b = 2;
            break;
        case XB1:
            b = 3;
            break;
        case XB2:
            b = 4;
            break;
        default:
            return;
    }
    bsd_button(b, false);
}

/// @brief rotate the wheel for `scale`*Delta
void wheel(wheel_rotations rotation, double scale) {
    if (scale == 0.0) return;
    if (!platform_uinput_setup()) return;

    // Windows WHEEL_DELTA == 120; one wheel notch == 1 unit (same rounding
    // as the Linux and macOS backends)
    int delta;
    if (scale >= 1.0 || scale <= -1.0)
        delta = static_cast<int>(scale + (scale > 0.0 ? 0.5 : -0.5));
    else if (scale > 0.0)
        delta = 1;
    else
        delta = -1;

    if (rotation == WD) delta = -delta;

    bsd_wheel(delta);
}

}  // namespace mouse

