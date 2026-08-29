#pragma once
// Self-contained evdev/uinput definitions shared by the FreeBSD and
// DragonFly backends (src/bsd.cpp).
//
// FreeBSD and DragonFly both ship Linux-compatible evdev, but neither
// installs the kernel headers (sys/dev/evdev/*) into /usr/include for
// userland, so we spell out the ioctl numbers, event codes and the wire
// format here.  The values are identical on FreeBSD / DragonFly / Linux:
//   - ioctl numbers follow FreeBSD's sys/dev/evdev/input.h & uinput.h
//     (a Linux-compatible encoding, byte-identical to DragonFly's)
//   - event types/axes/buttons follow input-event-codes.h
//   - KEY_* key codes are the same as Linux/FreeBSD
//
// Only consumed by src/bsd.cpp on BSD targets, never on Windows/Linux/macOS.

#include <sys/ioccom.h>  // _IO/_IOW/_IOC/IOC_OUT (FreeBSD & DragonFly)
#include <sys/time.h>    // struct timeval
#include <cstdint>

// ---- evdev ioctl numbers (sys/dev/evdev/input.h) ----
#define EVIOCGBIT(type, len) _IOC(IOC_OUT, 'E', 0x20 + (type), len)
#define EVIOCGKEY(len)       _IOC(IOC_OUT, 'E', 0x18, len)

// ---- uinput ioctls (sys/dev/evdev/uinput.h, Linux-compatible) ----
#define UI_DEV_CREATE   _IO('U', 1)
#define UI_DEV_DESTROY  _IO('U', 2)
#define UI_SET_EVBIT    _IOW('U', 100, int)
#define UI_SET_KEYBIT   _IOW('U', 101, int)
#define UI_SET_RELBIT   _IOW('U', 102, int)
#define UI_DEV_SETUP    _IOW('U', 108, struct uinput_setup)

// ---- event types / sync (input-event-codes.h) ----
#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03
#define SYN_REPORT 0

// ---- relative axes ----
#define REL_X 0x00
#define REL_Y 0x01
#define REL_WHEEL 0x08

// ---- mouse buttons ----
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112
#define BTN_SIDE   0x113
#define BTN_EXTRA  0x114

// ---- misc ----
#define BUS_USB 3
// key-state query size: KEY_MAX (0x2ff) + 1
#define KEY_CNT 768

// ---- evdev wire format (input.h) — identical to Linux/FreeBSD/DragonFly ----
struct input_event {
    struct timeval time;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

// Linux-compatible device identity used by UI_DEV_SETUP (uinput.h)
struct input_id {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
};

struct uinput_setup {
    char name[80];  // UINPUT_MAX_NAME_SIZE
    struct input_id id;
    uint32_t ff_effects_max;
};

// ---- evdev key codes (input-event-codes.h) ----
// The subset referenced by evdev_keys_map.hpp; values identical on
// Linux/FreeBSD/DragonFly.
#define KEY_1 2
#define KEY_2 3
#define KEY_3 4
#define KEY_4 5
#define KEY_5 6
#define KEY_6 7
#define KEY_7 8
#define KEY_8 9
#define KEY_9 10
#define KEY_0 11
#define KEY_A 30
#define KEY_B 48
#define KEY_C 46
#define KEY_D 32
#define KEY_E 18
#define KEY_F 33
#define KEY_G 34
#define KEY_H 35
#define KEY_I 23
#define KEY_J 36
#define KEY_K 37
#define KEY_L 38
#define KEY_M 50
#define KEY_N 49
#define KEY_O 24
#define KEY_P 25
#define KEY_Q 16
#define KEY_R 19
#define KEY_S 31
#define KEY_T 20
#define KEY_U 22
#define KEY_V 47
#define KEY_W 17
#define KEY_X 45
#define KEY_Y 21
#define KEY_Z 44
#define KEY_MINUS 12
#define KEY_EQUAL 13
#define KEY_BACKSPACE 14
#define KEY_TAB 15
#define KEY_LEFTBRACE 26
#define KEY_RIGHTBRACE 27
#define KEY_ENTER 28
#define KEY_LEFTCTRL 29
#define KEY_SEMICOLON 39
#define KEY_APOSTROPHE 40
#define KEY_GRAVE 41
#define KEY_LEFTSHIFT 42
#define KEY_BACKSLASH 43
#define KEY_COMMA 51
#define KEY_DOT 52
#define KEY_SLASH 53
#define KEY_RIGHTSHIFT 54
#define KEY_LEFTALT 56
#define KEY_SPACE 57
#define KEY_CAPSLOCK 58
#define KEY_F1 59
#define KEY_F2 60
#define KEY_F3 61
#define KEY_F4 62
#define KEY_F5 63
#define KEY_F6 64
#define KEY_F7 65
#define KEY_F8 66
#define KEY_F9 67
#define KEY_F10 68
#define KEY_F11 87
#define KEY_F12 88
#define KEY_ESC 1
#define KEY_INSERT 110
#define KEY_DELETE 111
#define KEY_RIGHT 106
#define KEY_LEFT 105
#define KEY_DOWN 108
#define KEY_UP 103
#define KEY_PAGEUP 104
#define KEY_PAGEDOWN 109
#define KEY_HOME 102
#define KEY_END 107
#define KEY_SCROLLLOCK 70
#define KEY_NUMLOCK 69
#define KEY_SYSRQ 99
#define KEY_PAUSE 119
#define KEY_RIGHTCTRL 97
#define KEY_RIGHTALT 100
#define KEY_LEFTMETA 125
#define KEY_RIGHTMETA 126
#define KEY_MENU 139
#define KEY_KP0 82
#define KEY_KP1 79
#define KEY_KP2 80
#define KEY_KP3 81
#define KEY_KP4 75
#define KEY_KP5 76
#define KEY_KP6 77
#define KEY_KP7 71
#define KEY_KP8 72
#define KEY_KP9 73
#define KEY_KPDOT 83
#define KEY_KPSLASH 98
#define KEY_KPASTERISK 55
#define KEY_KPMINUS 74
#define KEY_KPPLUS 78
#define KEY_KPENTER 96
#define KEY_KPEQUAL 117
