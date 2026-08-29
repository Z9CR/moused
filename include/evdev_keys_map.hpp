#pragma once
// X-Macro: single source of truth mapping Linux/FreeBSD evdev key codes
// (KEY_*, see <linux/input.h> on Linux / <dev/evdev/input-event-codes.h> on
// FreeBSD — both use the same numbers) to the keyboard::keys enum
// (include/enums_list.hpp).
//
// Used by src/linux.cpp and the FreeBSD backend of src/bsd.cpp to generate
// BOTH directions of the mapping from this one list:
//   - evdev key code -> keys : switch cases (linux_keycode_to_keys /
//                              bsd_keycode_to_keys)
//   - keys -> evdev key code : reverse lookup table (keys_to_linux_keycode /
//                              keys_to_bsd_keycode)
//
// Keys that have no evdev counterpart are simply absent from the list
// (they map to keys::NONE / -1, as before).
//
// ITEM(evdev_keycode, keys_enumerator)
#define EVDEV_KEYS_LIST(ITEM)               \
    /* numbers row */                       \
    ITEM(KEY_1, keyboard::keys::ONE)        \
    ITEM(KEY_2, keyboard::keys::TWO)        \
    ITEM(KEY_3, keyboard::keys::THREE)      \
    ITEM(KEY_4, keyboard::keys::FOUR)       \
    ITEM(KEY_5, keyboard::keys::FIVE)       \
    ITEM(KEY_6, keyboard::keys::SIX)        \
    ITEM(KEY_7, keyboard::keys::SEVEN)      \
    ITEM(KEY_8, keyboard::keys::EIGHT)      \
    ITEM(KEY_9, keyboard::keys::NINE)       \
    ITEM(KEY_0, keyboard::keys::ZERO)       \
    /* letters (evdev codes are NOT ASCII) */ \
    ITEM(KEY_A, keyboard::keys::A)          \
    ITEM(KEY_B, keyboard::keys::B)          \
    ITEM(KEY_C, keyboard::keys::C)          \
    ITEM(KEY_D, keyboard::keys::D)          \
    ITEM(KEY_E, keyboard::keys::E)          \
    ITEM(KEY_F, keyboard::keys::F)          \
    ITEM(KEY_G, keyboard::keys::G)          \
    ITEM(KEY_H, keyboard::keys::H)          \
    ITEM(KEY_I, keyboard::keys::I)          \
    ITEM(KEY_J, keyboard::keys::J)          \
    ITEM(KEY_K, keyboard::keys::K)          \
    ITEM(KEY_L, keyboard::keys::L)          \
    ITEM(KEY_M, keyboard::keys::M)          \
    ITEM(KEY_N, keyboard::keys::N)          \
    ITEM(KEY_O, keyboard::keys::O)          \
    ITEM(KEY_P, keyboard::keys::P)          \
    ITEM(KEY_Q, keyboard::keys::Q)          \
    ITEM(KEY_R, keyboard::keys::R)          \
    ITEM(KEY_S, keyboard::keys::S)          \
    ITEM(KEY_T, keyboard::keys::T)          \
    ITEM(KEY_U, keyboard::keys::U)          \
    ITEM(KEY_V, keyboard::keys::V)          \
    ITEM(KEY_W, keyboard::keys::W)          \
    ITEM(KEY_X, keyboard::keys::X)          \
    ITEM(KEY_Y, keyboard::keys::Y)          \
    ITEM(KEY_Z, keyboard::keys::Z)          \
    /* punctuation & editing */             \
    ITEM(KEY_MINUS, keyboard::keys::MINUS)  \
    ITEM(KEY_EQUAL, keyboard::keys::EQUAL)  \
    ITEM(KEY_BACKSPACE, keyboard::keys::BACKSPACE) \
    ITEM(KEY_TAB, keyboard::keys::TAB)      \
    ITEM(KEY_LEFTBRACE, keyboard::keys::LEFT_BRACKET) \
    ITEM(KEY_RIGHTBRACE, keyboard::keys::RIGHT_BRACKET) \
    ITEM(KEY_ENTER, keyboard::keys::ENTER)  \
    ITEM(KEY_LEFTCTRL, keyboard::keys::LEFT_CONTROL) \
    ITEM(KEY_SEMICOLON, keyboard::keys::SEMICOLON) \
    ITEM(KEY_APOSTROPHE, keyboard::keys::APOSTROPHE) \
    ITEM(KEY_GRAVE, keyboard::keys::GRAVE)  \
    ITEM(KEY_LEFTSHIFT, keyboard::keys::LEFT_SHIFT) \
    ITEM(KEY_BACKSLASH, keyboard::keys::BACKSLASH) \
    ITEM(KEY_COMMA, keyboard::keys::COMMA)  \
    ITEM(KEY_DOT, keyboard::keys::PERIOD)   \
    ITEM(KEY_SLASH, keyboard::keys::SLASH)  \
    ITEM(KEY_RIGHTSHIFT, keyboard::keys::RIGHT_SHIFT) \
    ITEM(KEY_LEFTALT, keyboard::keys::LEFT_ALT) \
    ITEM(KEY_SPACE, keyboard::keys::SPACE)  \
    ITEM(KEY_CAPSLOCK, keyboard::keys::CAPS_LOCK) \
    /* function keys */                     \
    ITEM(KEY_F1, keyboard::keys::F1)        \
    ITEM(KEY_F2, keyboard::keys::F2)        \
    ITEM(KEY_F3, keyboard::keys::F3)        \
    ITEM(KEY_F4, keyboard::keys::F4)        \
    ITEM(KEY_F5, keyboard::keys::F5)        \
    ITEM(KEY_F6, keyboard::keys::F6)        \
    ITEM(KEY_F7, keyboard::keys::F7)        \
    ITEM(KEY_F8, keyboard::keys::F8)        \
    ITEM(KEY_F9, keyboard::keys::F9)        \
    ITEM(KEY_F10, keyboard::keys::F10)      \
    ITEM(KEY_F11, keyboard::keys::F11)      \
    ITEM(KEY_F12, keyboard::keys::F12)      \
    /* navigation & locks */                \
    ITEM(KEY_ESC, keyboard::keys::ESCAPE)   \
    ITEM(KEY_INSERT, keyboard::keys::INSERT) \
    ITEM(KEY_DELETE, keyboard::keys::DELETE) \
    ITEM(KEY_RIGHT, keyboard::keys::RIGHT)  \
    ITEM(KEY_LEFT, keyboard::keys::LEFT)    \
    ITEM(KEY_DOWN, keyboard::keys::DOWN)    \
    ITEM(KEY_UP, keyboard::keys::UP)        \
    ITEM(KEY_PAGEUP, keyboard::keys::PAGE_UP) \
    ITEM(KEY_PAGEDOWN, keyboard::keys::PAGE_DOWN) \
    ITEM(KEY_HOME, keyboard::keys::HOME)    \
    ITEM(KEY_END, keyboard::keys::END)      \
    ITEM(KEY_SCROLLLOCK, keyboard::keys::SCROLL_LOCK) \
    ITEM(KEY_NUMLOCK, keyboard::keys::NUM_LOCK) \
    ITEM(KEY_SYSRQ, keyboard::keys::PRINT_SCREEN) \
    ITEM(KEY_PAUSE, keyboard::keys::PAUSE)  \
    /* right modifiers */                   \
    ITEM(KEY_RIGHTCTRL, keyboard::keys::RIGHT_CONTROL) \
    ITEM(KEY_RIGHTALT, keyboard::keys::RIGHT_ALT) \
    ITEM(KEY_LEFTMETA, keyboard::keys::LEFT_SUPER) \
    ITEM(KEY_RIGHTMETA, keyboard::keys::RIGHT_SUPER) \
    ITEM(KEY_MENU, keyboard::keys::KB_MENU) \
    /* keypad */                            \
    ITEM(KEY_KP0, keyboard::keys::KP_0)     \
    ITEM(KEY_KP1, keyboard::keys::KP_1)     \
    ITEM(KEY_KP2, keyboard::keys::KP_2)     \
    ITEM(KEY_KP3, keyboard::keys::KP_3)     \
    ITEM(KEY_KP4, keyboard::keys::KP_4)     \
    ITEM(KEY_KP5, keyboard::keys::KP_5)     \
    ITEM(KEY_KP6, keyboard::keys::KP_6)     \
    ITEM(KEY_KP7, keyboard::keys::KP_7)     \
    ITEM(KEY_KP8, keyboard::keys::KP_8)     \
    ITEM(KEY_KP9, keyboard::keys::KP_9)     \
    ITEM(KEY_KPDOT, keyboard::keys::KP_DECIMAL) \
    ITEM(KEY_KPSLASH, keyboard::keys::KP_DIVIDE) \
    ITEM(KEY_KPASTERISK, keyboard::keys::KP_MULTIPLY) \
    ITEM(KEY_KPMINUS, keyboard::keys::KP_SUBTRACT) \
    ITEM(KEY_KPPLUS, keyboard::keys::KP_ADD) \
    ITEM(KEY_KPENTER, keyboard::keys::KP_ENTER) \
    ITEM(KEY_KPEQUAL, keyboard::keys::KP_EQUAL)
