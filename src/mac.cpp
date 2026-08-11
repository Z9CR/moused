// macOS backend for moused.
//
// Uses CoreGraphics (CGEvent*) to synthesize mouse input and
// ApplicationServices (AXIsProcessTrusted) to verify the Accessibility
// permission macOS requires before a process may post synthetic events.
//
// Note: macOS global display coordinates share the same origin convention as
// the rest of moused — (0, 0) is the top-left of the main display, x grows to
// the right and y grows down — so no coordinate transform is needed.
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#include <adapter.hpp>
#include <chrono>
#include <cmath>
#include <config.hpp>
#include <thread>
#include <utils.hpp>

using mouse::mouse_btns;
using mouse::wheel_rotations;

constexpr double PI = 3.14159265358979323846;

namespace {
using keyboard::keys;

// ---------------------------------------------------------------------
// X-macro: single source of truth for the keys <-> macOS VK mapping.
// ITEM(name, vk, code)
//   name : keyboard::keys enumerator (see include/enums_list.hpp)
//   vk   : enumerator of the local VK enum generated below
//   code : macOS virtual key code (Carbon kVK_* values)
//
// Row order = get_key_pressed() scan priority: letters, digits,
// punctuation, modifiers, editing, navigation, locks, F-keys, keypad.
// Keys with no macOS counterpart use VK::UNMAPPED (e.g. KB_MENU).
// ---------------------------------------------------------------------
#define KEYS_VK_LIST(ITEM)                                        \
    /* letters */                                                 \
    ITEM(A, ANSI_A, 0x00)                                         \
    ITEM(B, ANSI_B, 0x0B)                                         \
    ITEM(C, ANSI_C, 0x08)                                         \
    ITEM(D, ANSI_D, 0x02)                                         \
    ITEM(E, ANSI_E, 0x0E)                                         \
    ITEM(F, ANSI_F, 0x03)                                         \
    ITEM(G, ANSI_G, 0x05)                                         \
    ITEM(H, ANSI_H, 0x04)                                         \
    ITEM(I, ANSI_I, 0x22)                                         \
    ITEM(J, ANSI_J, 0x26)                                         \
    ITEM(K, ANSI_K, 0x28)                                         \
    ITEM(L, ANSI_L, 0x25)                                         \
    ITEM(M, ANSI_M, 0x2E)                                         \
    ITEM(N, ANSI_N, 0x2D)                                         \
    ITEM(O, ANSI_O, 0x1F)                                         \
    ITEM(P, ANSI_P, 0x23)                                         \
    ITEM(Q, ANSI_Q, 0x0C)                                         \
    ITEM(R, ANSI_R, 0x0F)                                         \
    ITEM(S, ANSI_S, 0x01)                                         \
    ITEM(T, ANSI_T, 0x11)                                         \
    ITEM(U, ANSI_U, 0x20)                                         \
    ITEM(V, ANSI_V, 0x09)                                         \
    ITEM(W, ANSI_W, 0x0D)                                         \
    ITEM(X, ANSI_X, 0x07)                                         \
    ITEM(Y, ANSI_Y, 0x10)                                         \
    ITEM(Z, ANSI_Z, 0x06)                                         \
    /* digits */                                                  \
    ITEM(ZERO, ANSI_0, 0x1D)                                      \
    ITEM(ONE, ANSI_1, 0x12)                                       \
    ITEM(TWO, ANSI_2, 0x13)                                       \
    ITEM(THREE, ANSI_3, 0x14)                                     \
    ITEM(FOUR, ANSI_4, 0x15)                                      \
    ITEM(FIVE, ANSI_5, 0x17)                                      \
    ITEM(SIX, ANSI_6, 0x16)                                       \
    ITEM(SEVEN, ANSI_7, 0x1A)                                     \
    ITEM(EIGHT, ANSI_8, 0x1C)                                     \
    ITEM(NINE, ANSI_9, 0x19)                                      \
    /* punctuation */                                             \
    ITEM(APOSTROPHE, ANSI_Quote, 0x27)                            \
    ITEM(COMMA, ANSI_Comma, 0x2B)                                 \
    ITEM(MINUS, ANSI_Minus, 0x1B)                                 \
    ITEM(PERIOD, ANSI_Period, 0x2F)                               \
    ITEM(SLASH, ANSI_Slash, 0x2C)                                 \
    ITEM(SEMICOLON, ANSI_Semicolon, 0x29)                         \
    ITEM(EQUAL, ANSI_Equal, 0x18)                                 \
    ITEM(LEFT_BRACKET, ANSI_LeftBracket, 0x21)                    \
    ITEM(BACKSLASH, ANSI_Backslash, 0x2A)                         \
    ITEM(RIGHT_BRACKET, ANSI_RightBracket, 0x1E)                  \
    ITEM(GRAVE, ANSI_Grave, 0x32)                                 \
    ITEM(SPACE, Space, 0x31)                                      \
    /* modifiers */                                               \
    ITEM(LEFT_SHIFT, Shift, 0x38)                                 \
    ITEM(LEFT_CONTROL, Control, 0x3B)                             \
    ITEM(LEFT_ALT, Option, 0x3A)                                  \
    ITEM(LEFT_SUPER, Command, 0x37)                               \
    ITEM(RIGHT_SHIFT, RightShift, 0x3C)                           \
    ITEM(RIGHT_CONTROL, RightControl, 0x3E)                       \
    ITEM(RIGHT_ALT, RightOption, 0x3D)                            \
    ITEM(RIGHT_SUPER, RightCommand, 0x36)                         \
    ITEM(CAPS_LOCK, CapsLock, 0x39)                               \
    /* editing */                                                 \
    ITEM(ESCAPE, Escape, 0x35)                                    \
    ITEM(ENTER, Return, 0x24)                                     \
    ITEM(TAB, Tab, 0x30)                                          \
    ITEM(BACKSPACE, Delete, 0x33) /* macOS Delete=Backspace */    \
    ITEM(INSERT, Help, 0x72)      /* macOS has no Insert */       \
    ITEM(DELETE, ForwardDelete, 0x75)                             \
    /* navigation */                                              \
    ITEM(RIGHT, Right, 0x7C)                                      \
    ITEM(LEFT, Left, 0x7B)                                        \
    ITEM(DOWN, Down, 0x7D)                                        \
    ITEM(UP, Up, 0x7E)                                            \
    ITEM(PAGE_UP, PageUp, 0x74)                                   \
    ITEM(PAGE_DOWN, PageDown, 0x79)                               \
    ITEM(HOME, Home, 0x73)                                        \
    ITEM(END, End, 0x77)                                          \
    /* locks: Apple full-size keyboards surface PrintScreen/      \
       ScrollLock/Pause as F13/F14/F15; NumLock = keypad Clear */ \
    ITEM(PRINT_SCREEN, F13, 0x69)                                 \
    ITEM(SCROLL_LOCK, F14, 0x6B)                                  \
    ITEM(PAUSE, F15, 0x71)                                        \
    ITEM(NUM_LOCK, KeypadClear, 0x47)                             \
    /* function keys */                                           \
    ITEM(F1, F1, 0x7A)                                            \
    ITEM(F2, F2, 0x78)                                            \
    ITEM(F3, F3, 0x63)                                            \
    ITEM(F4, F4, 0x76)                                            \
    ITEM(F5, F5, 0x60)                                            \
    ITEM(F6, F6, 0x61)                                            \
    ITEM(F7, F7, 0x62)                                            \
    ITEM(F8, F8, 0x64)                                            \
    ITEM(F9, F9, 0x65)                                            \
    ITEM(F10, F10, 0x6D)                                          \
    ITEM(F11, F11, 0x67)                                          \
    ITEM(F12, F12, 0x6F)                                          \
    /* keypad */                                                  \
    ITEM(KP_0, Keypad0, 0x52)                                     \
    ITEM(KP_1, Keypad1, 0x53)                                     \
    ITEM(KP_2, Keypad2, 0x54)                                     \
    ITEM(KP_3, Keypad3, 0x55)                                     \
    ITEM(KP_4, Keypad4, 0x56)                                     \
    ITEM(KP_5, Keypad5, 0x57)                                     \
    ITEM(KP_6, Keypad6, 0x58)                                     \
    ITEM(KP_7, Keypad7, 0x59)                                     \
    ITEM(KP_8, Keypad8, 0x5B)                                     \
    ITEM(KP_9, Keypad9, 0x5C)                                     \
    ITEM(KP_DECIMAL, KeypadDecimal, 0x41)                         \
    ITEM(KP_DIVIDE, KeypadDivide, 0x4B)                           \
    ITEM(KP_MULTIPLY, KeypadMultiply, 0x43)                       \
    ITEM(KP_SUBTRACT, KeypadMinus, 0x4E)                          \
    ITEM(KP_ADD, KeypadPlus, 0x45)                                \
    ITEM(KP_ENTER, KeypadEnter, 0x4C)                             \
    ITEM(KP_EQUAL, KeypadEquals, 0x51)                            \
    /* no macOS counterpart */                                    \
    ITEM(KB_MENU, UNMAPPED, 0xFF)

// macOS virtual key codes, generated from the X-macro above.
enum class VK : CGKeyCode {
#define KEYS_VK_ITEM(name, vk, code) vk = code,
    KEYS_VK_LIST(KEYS_VK_ITEM)
#undef KEYS_VK_ITEM
};

struct KeyMap {
    keys key;
    VK vk;
};

constexpr KeyMap kKeyTable[] = {
#define KEYS_VK_ITEM(name, vk, code) {keys::name, VK::vk},
    KEYS_VK_LIST(KEYS_VK_ITEM)
#undef KEYS_VK_ITEM
};
#undef KEYS_VK_LIST

// keys -> macOS VK, or -1 when the key has no macOS counterpart.
CGKeyCode key_to_vk(keys key) {
    for (const auto& e : kKeyTable)
        if (e.key == key) {
            if (e.vk == VK::UNMAPPED) return static_cast<CGKeyCode>(-1);
            return static_cast<CGKeyCode>(e.vk);
        }
    return static_cast<CGKeyCode>(-1);  // not in table at all
}

// Posting synthetic input needs the Accessibility permission.
//
// Unlike Linux' polkit there is no way for an app to pop-up a dialog that
// grants the permission itself — the user must enable it in System Settings.
// macOS' closest equivalent (the official, supported way) is to ask the
// system to show its own one-shot prompt via
// `AXIsProcessTrustedWithOptions(kAXTrustedCheckOptionPrompt)`: the system
// shows a "moused would like to control this computer" dialog with an
// "Open System Settings" button, mirroring the polkit UX we have on Linux.
bool posting_ready() {
    static bool prompted = false;

    if (AXIsProcessTrusted()) return true;

    if (!prompted) {
        prompted = true;
        // Ask the system to display its Accessibility permission prompt.
        CFMutableDictionaryRef options = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        if (options) {
            CFDictionarySetValue(options, kAXTrustedCheckOptionPrompt,
                                 kCFBooleanTrue);
            // With the prompt option set the return value is not a reliable
            // trust check (it may return false even when already trusted),
            // so we re-query with plain AXIsProcessTrusted() below.
            AXIsProcessTrustedWithOptions(options);
            CFRelease(options);
        }
        log_msg(
            "moused: macOS Accessibility permission is required.\n"
            "A system dialog should have appeared - click \"Open System "
            "Settings\" (or go to System Settings -> Privacy & Security -> "
            "Accessibility), enable moused, then retry.\n"
            "Note: the app cannot grant itself this permission; it must "
            "be enabled by the user.\n");
    }

    // If the user just enabled the permission the process is trusted again.
    return AXIsProcessTrusted();
}

// Current cursor position in global display coordinates.
CGPoint get_cursor_pos() {
    CGEventRef ev = CGEventCreate(nullptr);
    if (!ev) return CGPointMake(0, 0);
    CGPoint p = CGEventGetLocation(ev);
    CFRelease(ev);
    return p;
}

// Move the cursor to `pos` (absolute, global display coordinates).
void post_cursor(CGPoint pos) {
    CGEventRef ev = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, pos,
                                            kCGMouseButtonLeft);
    if (!ev) return;
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}

// Press or release a button at the current cursor position.
void post_mouse_button(CGEventType type, CGMouseButton button) {
    CGPoint pos = get_cursor_pos();
    CGEventRef ev = CGEventCreateMouseEvent(nullptr, type, pos, button);
    if (!ev) return;
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}

}  // namespace

namespace mouse {
/// @brief translate cursor
/// @param a the angle away from x+
/// @param distant how many PXs will cursor translate
void translate(angle a, int distant, unsigned int time_ms) {
    if (!posting_ready()) return;

    // a may be >= 360 || <= -360, so normalise it by folding (can't fmod a
    // double with %, matching the Windows backend).
    if (a >= 360.0)
        while (a -= 360.0, a >= 360.0) {
        }
    if (a <= -360.0)
        while (a += 360.0, a <= -360.0) {
        }

    double rad = a * PI / 180.0;
    CGPoint cur = get_cursor_pos();
    int dstx = static_cast<int>(cur.x) + static_cast<int>(cos(rad) * distant);
    int dsty = static_cast<int>(cur.y) + static_cast<int>(sin(rad) * distant);
    move_to(static_cast<unsigned int>(dstx), static_cast<unsigned int>(dsty),
            time_ms);
}

/// @brief translate cursor
/// @param a the angle away from x+
/// @param distant how many PXs will cursor translate
void translate(angle a, int distant) {
    if (!posting_ready()) return;

    if (a >= 360.0)
        while (a -= 360.0, a >= 360.0) {
        }
    if (a <= -360.0)
        while (a += 360.0, a <= -360.0) {
        }

    double rad = a * PI / 180.0;
    CGPoint cur = get_cursor_pos();
    int dstx = static_cast<int>(cur.x) + static_cast<int>(cos(rad) * distant);
    int dsty = static_cast<int>(cur.y) + static_cast<int>(sin(rad) * distant);
    move_to(static_cast<unsigned int>(dstx), static_cast<unsigned int>(dsty));
}

/// @brief move the mouse to (x, y) (right = x+, down = y +)
/// @param x: how many PXs away from left boarder
/// @param y: how many PXs away from up boarder
/// @param time_ms: time duration for the smooth movement, in ms
void move_to(unsigned int x, unsigned int y, unsigned int time_ms) {
    if (!posting_ready()) return;

    CGPoint start = get_cursor_pos();
    const CGFloat end_x = static_cast<CGFloat>(x);
    const CGFloat end_y = static_cast<CGFloat>(y);

    // guard against a zero/negative frame time from a broken config
    int frame_time = smoothmv_frametime;
    if (frame_time < 1) frame_time = 1;

    int frames = static_cast<int>(time_ms) / frame_time;
    if (frames < 1) frames = 1;

    // interpolate and post each frame
    for (int i = 1; i <= frames; ++i) {
        double t = static_cast<double>(i) / frames;
        CGFloat cur_x = start.x + (end_x - start.x) * t;
        CGFloat cur_y = start.y + (end_y - start.y) * t;
        post_cursor(CGPointMake(cur_x, cur_y));
        std::this_thread::sleep_for(std::chrono::milliseconds(frame_time));
    }
    // in case of float deviation
    move_to(x, y);
}

/// @brief move the mouse to (x, y) (right = x+, down = y +)
/// @param x: how many PXs away from left boarder
/// @param y: how many PXs away from up boarder
void move_to(unsigned int x, unsigned int y) {
    if (!posting_ready()) return;
    post_cursor(CGPointMake(static_cast<CGFloat>(x), static_cast<CGFloat>(y)));
}

/// @brief click the `btn`
void click(mouse_btns btn) {
    if (!posting_ready()) return;
    press(btn);
    release(btn);
}

/// @brief press the `btn`
void press(mouse_btns btn) {
    if (!posting_ready()) return;
    switch (btn) {
        case LMB:
            post_mouse_button(kCGEventLeftMouseDown, kCGMouseButtonLeft);
            break;
        case RMB:
            post_mouse_button(kCGEventRightMouseDown, kCGMouseButtonRight);
            break;
        case MMB:
            post_mouse_button(kCGEventOtherMouseDown, kCGMouseButtonCenter);
            break;
        case XB1:  // button 4
            post_mouse_button(kCGEventOtherMouseDown,
                              static_cast<CGMouseButton>(3));
            break;
        case XB2:  // button 5
            post_mouse_button(kCGEventOtherMouseDown,
                              static_cast<CGMouseButton>(4));
            break;
    }
}

/// @brief release the pressed `btn`
void release(mouse_btns btn) {
    if (!posting_ready()) return;
    switch (btn) {
        case LMB:
            post_mouse_button(kCGEventLeftMouseUp, kCGMouseButtonLeft);
            break;
        case RMB:
            post_mouse_button(kCGEventRightMouseUp, kCGMouseButtonRight);
            break;
        case MMB:
            post_mouse_button(kCGEventOtherMouseUp, kCGMouseButtonCenter);
            break;
        case XB1:  // button 4
            post_mouse_button(kCGEventOtherMouseUp,
                              static_cast<CGMouseButton>(3));
            break;
        case XB2:  // button 5
            post_mouse_button(kCGEventOtherMouseUp,
                              static_cast<CGMouseButton>(4));
            break;
    }
}

/// @brief routate the MMB for `scale`*Delta
void wheel(wheel_rotations rotation, double scale) {
    if (!posting_ready()) return;
    if (scale == 0.0) return;

    // Turn the Windows-style WHEEL_DELTA scale into notch counts
    // (1 line = 1 notch), same rounding as the Linux backend.
    int delta;
    if (scale >= 1.0 || scale <= -1.0)
        delta = static_cast<int>(scale + (scale > 0.0 ? 0.5 : -0.5));
    else if (scale > 0.0)
        delta = 1;
    else
        delta = -1;

    if (rotation == WD) delta = -delta;

    CGEventRef ev = CGEventCreateScrollWheelEvent(
        nullptr, kCGScrollEventUnitLine, 1, delta);
    if (!ev) return;
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}

}  // namespace mouse

namespace keyboard {
keys get_key_pressed() {
    for (const auto& e : kKeyTable) {
        if (e.vk == VK::UNMAPPED) continue;  // no macOS counterpart
        if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState,
                                  static_cast<CGKeyCode>(e.vk)))
            return e.key;
    }
    return keys::NONE;
}

bool is_key_pressed(keys key) {
    if (key == keys::NONE) return false;
    CGKeyCode vk = key_to_vk(key);
    if (vk == static_cast<CGKeyCode>(-1)) return false;  // unmapped
    return CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, vk);
}
}  // namespace keyboard
