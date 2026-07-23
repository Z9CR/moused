#include <adapter.hpp>
#include <win.hpp>
#include <definitions.hpp>
#include <Windows.h>


//  (0, 0)-----------(65535, 0    )--x+
//  |   the screen is     |
//  |     divided into    |
//  |      65535*65535    |
//  (0, 65535)-------(65535, 65535)
//  y+
    
/// @brief move the mouse with direction and distance
/// @param d: the direction
/// @param distance: how many PXs you want to move
void mouse_translate(direction d, int distance) {

}

/// @brief move the mouse to (x, y) (right = x+, down = y +)
void mouse_move_to(int x, int y) {

}

/// @brief click the `btn`
void mouse_click(mouse_btns btn) {
    INPUT inputs[2]{};  // 2 event in need
    inputs[0].type = INPUT_MOUSE;
    inputs[1].type = INPUT_MOUSE;
    DWORD down_flag{}, up_flag{};
    switch (btn) {
        case LMB:
            down_flag = MOUSEEVENTF_LEFTDOWN;
            up_flag = MOUSEEVENTF_LEFTUP;
            break;
        case RMB:
            down_flag = MOUSEEVENTF_RIGHTDOWN;
            up_flag = MOUSEEVENTF_RIGHTUP;
            break;
        case MMB:
            down_flag = MOUSEEVENTF_MIDDLEDOWN;
            up_flag = MOUSEEVENTF_MIDDLEUP;
            break;
        case XB1:
            down_flag = MOUSEEVENTF_XDOWN;
            up_flag = MOUSEEVENTF_XUP;
            inputs[0].mi.mouseData = XBUTTON1;
            inputs[1].mi.mouseData = XBUTTON1;
            break;
        case XB2:
            down_flag = MOUSEEVENTF_XDOWN;
            up_flag = MOUSEEVENTF_XUP;
            inputs[0].mi.mouseData = XBUTTON2;
            inputs[1].mi.mouseData = XBUTTON2;
            break;
    };
    inputs[0].mi.dwFlags = down_flag;
    inputs[1].mi.dwFlags = up_flag;
    SendInput(2, inputs, sizeof(INPUT));
}

/// @brief press the `btn`
void mouse_press(mouse_btns btn) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    switch (btn) {
        case LMB:
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            break;
        case RMB:
            input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
            break;
        case MMB:
            input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
            break;
        case XB1:
            input.mi.dwFlags = MOUSEEVENTF_XDOWN;
            input.mi.mouseData = XBUTTON1;
            break;
        case XB2:
            input.mi.dwFlags = MOUSEEVENTF_XDOWN;
            input.mi.mouseData = XBUTTON2;
            break;
    };
    SendInput(1, &input, sizeof(INPUT));
}

/// @brief release the pressed `btn`
void mouse_release(mouse_btns btn) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    switch (btn) {
        case LMB:
            input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            break;
        case RMB:
            input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
            break;
        case MMB:
            input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
            break;
        case XB1:
            input.mi.dwFlags = MOUSEEVENTF_XUP;
            input.mi.mouseData = XBUTTON1;
            break;
        case XB2:
            input.mi.dwFlags = MOUSEEVENTF_XUP;
            input.mi.mouseData = XBUTTON2;
            break;
    };
    SendInput(1, &input, sizeof(INPUT));
}

/// @brief routate the MMB for `scale`*Delta
void mouse_wheel(wheel_rotations rotation, uf64 scale) {
    
}