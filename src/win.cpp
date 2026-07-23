#include <adapter.hpp>
#include <win.hpp>
#include <definitions.hpp>
#include <config.hpp>
#include <Windows.h>

enum axis {
    X = 0,
    Y = 1
};

// attention! you cant get point (monitor_width, monitor_height)
// it is over bound
const int monitor_width = GetSystemMetrics(SM_CXSCREEN);
const int monitor_height = GetSystemMetrics(SM_CYSCREEN);

int px2pos(int px, axis d) {
    if (d == X)
        return MulDiv(px, 65535, monitor_width -1);
    else 
        return MulDiv(px, 65535, monitor_height -1);
}

int pos2px(int pos, axis d) {
    if (d == X)
        return MulDiv(monitor_width -1, pos, 65535);
    else 
        return MulDiv(monitor_height -1, pos, 65535);
}

//  (0, 0)-----------(65535, 0    )--x+
//  |   the screen is     |
//  |     divided into    |
//  |      65535*65535    |
//  (0, 65535)-------(65535, 65535)
//  y+
    


/// @brief move the mouse to (x, y) (right = x+, down = y +)
/// @param x: how many PXs away from left boarder
/// @param y: how many PXs away from up boarder
/// @param time_ms: time duration for the smooth movement, in ms
void mouse::move_to(unsigned int x, unsigned int y, unsigned int time_ms) {
    // get current position
    POINT cur;
    GetCursorPos(&cur);
    int start_x = cur.x;
    int start_y = cur.y;

    // how many frames
    int frames = time_ms / smoothmv_frametime;
    if (frames < 1) frames = 1;

    // interpolate and send each frame
    for (int i = 1; i <= frames; ++i) {
        double t = static_cast<double>(i) / frames;
        int cur_x = start_x + (static_cast<int>(x) - start_x) * t;
        int cur_y = start_y + (static_cast<int>(y) - start_y) * t;

        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        input.mi.dx = px2pos(cur_x, X);
        input.mi.dy = px2pos(cur_y, Y);
        SendInput(1, &input, sizeof(INPUT));

        Sleep(smoothmv_frametime);
    }
}

/// @brief move the mouse to (x, y) (right = x+, down = y +)
/// @param x: how many PXs away from left boarder
/// @param y: how many PXs away from up boarder
void mouse::move_to(unsigned int x, unsigned int y) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    // sub 1, because px starts with 0
    input.mi.dx = px2pos(x, X);
    input.mi.dy = px2pos(y, Y);
    SendInput(1, &input, sizeof(INPUT));
}

/// @brief click the `btn`
void mouse::click(mouse_btns btn) {
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
void mouse::press(mouse_btns btn) {
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
void mouse::release(mouse_btns btn) {
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
void mouse::wheel(wheel_rotations rotation, uf64 scale) {
    
}