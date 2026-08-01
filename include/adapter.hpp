/*
 * declare funcs here, impl them in diff .cpp for cross platform
 */
#pragma once

/* if angle = 0,
 * it will point to x+ direction
 * which is the right direction in physical world
 * if angle > 0,
 * it will point to the direction CW rotate `angle` degs from x+
 * if angle < 0,
 * it will point to the direction CCW rotate `angle` degs from x+
 */
typedef double angle;

#include <enums_list.hpp>

namespace mouse
{

    enum mouse_btns
    {
#define MOUSE_BTN_ITEM(name, value) name = value,
        MOUSE_BTN_LIST(MOUSE_BTN_ITEM)
#undef MOUSE_BTN_ITEM
    };

    enum wheel_rotations
    {
#define WHEEL_ROTATION_ITEM(name, value) name = value,
        WHEEL_ROTATION_LIST(WHEEL_ROTATION_ITEM)
#undef WHEEL_ROTATION_ITEM
    };

    /// @brief translate cursor
    /// @param a the angle away from x+
    /// @param distant how many PXs will cursor translate
    void translate(angle a, int distant);

    /// @brief translate cursor
    /// @param a the angle away from x+
    /// @param distant how many PXs will cursor translate
    void translate(angle a, int distant, unsigned int time_ms);

    /// @brief move the mouse to (x, y) (right = x+, down = y +)
    /// @param x: how many PXs away from left boarder
    /// @param y: how many PXs away from up boarder
    void move_to(unsigned int x, unsigned int y, unsigned int time_ms);

    /// @brief move the mouse to (x, y) (right = x+, down = y +)
    /// @param x: how many PXs away from left boarder
    /// @param y: how many PXs away from up boarder
    void move_to(unsigned int x, unsigned int y);

    /// @brief click the `btn`
    void click(mouse_btns btn);

    /// @brief press the `btn`
    void press(mouse_btns btn);

    /// @brief release the pressed `btn`
    void release(mouse_btns btn);

    /// @brief routate the MMB for `scale`*Delta
    void wheel(wheel_rotations rotation, double scale);
};

#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
/// Initialize platform input device (/dev/uinput, /dev/wsmouse etc.)
/// Must be called as root; after a successful call the fd stays valid
/// even after dropping privileges via seteuid().
bool platform_uinput_setup();

/// init keyboard capture prog to impl hotkey feat
bool platform_keyboard_capture_setup();
#endif

namespace keyboard
{
    // Keyboard keys (US keyboard layout)
    // NOTE: Use GetKeyPressed() to allow redefining required keys for alternative layouts
    enum class keys
    {
        NULL = 0, // Key: NULL, used for no key pressed
        // Alphanumeric keys
        APOSTROPHE = 39,    // Key: '
        COMMA = 44,         // Key: ,
        MINUS = 45,         // Key: -
        PERIOD = 46,        // Key: .
        SLASH = 47,         // Key: /
        ZERO = 48,          // Key: 0
        ONE = 49,           // Key: 1
        TWO = 50,           // Key: 2
        THREE = 51,         // Key: 3
        FOUR = 52,          // Key: 4
        FIVE = 53,          // Key: 5
        SIX = 54,           // Key: 6
        SEVEN = 55,         // Key: 7
        EIGHT = 56,         // Key: 8
        NINE = 57,          // Key: 9
        SEMICOLON = 59,     // Key: ;
        EQUAL = 61,         // Key: =
        A = 65,             // Key: A | a
        B = 66,             // Key: B | b
        C = 67,             // Key: C | c
        D = 68,             // Key: D | d
        E = 69,             // Key: E | e
        F = 70,             // Key: F | f
        G = 71,             // Key: G | g
        H = 72,             // Key: H | h
        I = 73,             // Key: I | i
        J = 74,             // Key: J | j
        K = 75,             // Key: K | k
        L = 76,             // Key: L | l
        M = 77,             // Key: M | m
        N = 78,             // Key: N | n
        O = 79,             // Key: O | o
        P = 80,             // Key: P | p
        Q = 81,             // Key: Q | q
        R = 82,             // Key: R | r
        S = 83,             // Key: S | s
        T = 84,             // Key: T | t
        U = 85,             // Key: U | u
        V = 86,             // Key: V | v
        W = 87,             // Key: W | w
        X = 88,             // Key: X | x
        Y = 89,             // Key: Y | y
        Z = 90,             // Key: Z | z
        LEFT_BRACKET = 91,  // Key: [
        BACKSLASH = 92,     // Key: '\'
        RIGHT_BRACKET = 93, // Key: ]
        GRAVE = 96,         // Key: `
        // Function keys
        SPACE = 32,          // Key: Space
        ESCAPE = 256,        // Key: Esc
        ENTER = 257,         // Key: Enter
        TAB = 258,           // Key: Tab
        BACKSPACE = 259,     // Key: Backspace
        INSERT = 260,        // Key: Ins
        DELETE = 261,        // Key: Del
        RIGHT = 262,         // Key: Cursor right
        LEFT = 263,          // Key: Cursor left
        DOWN = 264,          // Key: Cursor down
        UP = 265,            // Key: Cursor up
        PAGE_UP = 266,       // Key: Page up
        PAGE_DOWN = 267,     // Key: Page down
        HOME = 268,          // Key: Home
        END = 269,           // Key: End
        CAPS_LOCK = 280,     // Key: Caps lock
        SCROLL_LOCK = 281,   // Key: Scroll down
        NUM_LOCK = 282,      // Key: Num lock
        PRINT_SCREEN = 283,  // Key: Print screen
        PAUSE = 284,         // Key: Pause
        F1 = 290,            // Key: F1
        F2 = 291,            // Key: F2
        F3 = 292,            // Key: F3
        F4 = 293,            // Key: F4
        F5 = 294,            // Key: F5
        F6 = 295,            // Key: F6
        F7 = 296,            // Key: F7
        F8 = 297,            // Key: F8
        F9 = 298,            // Key: F9
        F10 = 299,           // Key: F10
        F11 = 300,           // Key: F11
        F12 = 301,           // Key: F12
        LEFT_SHIFT = 340,    // Key: Shift left
        LEFT_CONTROL = 341,  // Key: Control left
        LEFT_ALT = 342,      // Key: Alt left
        LEFT_SUPER = 343,    // Key: Super left
        RIGHT_SHIFT = 344,   // Key: Shift right
        RIGHT_CONTROL = 345, // Key: Control right
        RIGHT_ALT = 346,     // Key: Alt right
        RIGHT_SUPER = 347,   // Key: Super right
        KB_MENU = 348,       // Key: KB menu
        // Keypad keys
        KP_0 = 320,        // Key: Keypad 0
        KP_1 = 321,        // Key: Keypad 1
        KP_2 = 322,        // Key: Keypad 2
        KP_3 = 323,        // Key: Keypad 3
        KP_4 = 324,        // Key: Keypad 4
        KP_5 = 325,        // Key: Keypad 5
        KP_6 = 326,        // Key: Keypad 6
        KP_7 = 327,        // Key: Keypad 7
        KP_8 = 328,        // Key: Keypad 8
        KP_9 = 329,        // Key: Keypad 9
        KP_DECIMAL = 330,  // Key: Keypad .
        KP_DIVIDE = 331,   // Key: Keypad /
        KP_MULTIPLY = 332, // Key: Keypad *
        KP_SUBTRACT = 333, // Key: Keypad -
        KP_ADD = 334,      // Key: Keypad +
        KP_ENTER = 335,    // Key: Keypad Enter
        KP_EQUAL = 336,    // Key: Keypad =
    };

    keys get_key_pressed();

    bool is_key_pressed(keys key);
}