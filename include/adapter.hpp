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

// The enumerator `DELETE` below collides with Windows.h's `DELETE` macro
// (0x00010000L), which wxWidgets pulls in transitively (ui.hpp -> wx/wx.h).
// Save & undefine it before the enums, restore the original value afterwards.
#ifdef DELETE
#define MOUSED_DELETE_BAK DELETE
#undef DELETE
#endif

namespace mouse {

enum mouse_btns {
#define MOUSE_BTN_ITEM(name, value) name = value,
    MOUSE_BTN_LIST(MOUSE_BTN_ITEM)
#undef MOUSE_BTN_ITEM
};

enum wheel_rotations {
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
};  // namespace mouse

#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__)
/// Initialize platform input device (/dev/uinput, /dev/wsmouse etc.)
/// Must be called as root; after a successful call the fd stays valid
/// even after dropping privileges via seteuid().
bool platform_uinput_setup();

/// init keyboard capture prog to impl hotkey feat
bool platform_keyboard_capture_setup();
#endif

namespace keyboard {
// Keyboard keys (US keyboard layout)
// NOTE: Use GetKeyPressed() to allow redefining required keys for alternative
// layouts
enum class keys {
#define KEYS_ITEM(name, value) name = value,
    KEYS_LIST(KEYS_ITEM)
#undef KEYS_ITEM
};

keys get_key_pressed();

bool is_key_pressed(keys key);
}  // namespace keyboard

#ifdef MOUSED_DELETE_BAK
#define DELETE MOUSED_DELETE_BAK
#undef MOUSED_DELETE_BAK
#endif
