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

// `NULL` is both a macro (defined by <cstddef>, pulled in transitively by
// many standard headers) and the name of an enumerator below.
// On MSVC the macro expands to `0`, which would break the `NULL = 0` member,
// so it must be undefined before the enum is declared. The original value is
// restored at the end of this header so we do not leak the change.
#ifdef NULL
#undef NULL
#define MOUSED_NULL_DEFINED
#endif

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
#define KEYS_ITEM(name, value) name = value,
        KEYS_LIST(KEYS_ITEM)
#undef KEYS_ITEM
    };

    keys get_key_pressed();

    bool is_key_pressed(keys key);
}

#ifdef MOUSED_NULL_DEFINED
#ifndef NULL
#define NULL 0
#endif
#undef MOUSED_NULL_DEFINED
#endif
