#include <adapter.hpp>
#include <config.hpp>
#include <Windows.h>
#undef NULL   // Windows.h defines NULL as 0 — we need it for keys::NULL
#undef DELETE // Windows.h defines DELETE as 0x00010000L — we need it for keys::DELETE
#include <cmath>

using mouse::mouse_btns;
using mouse::wheel_rotations;

constexpr double PI = 3.14159265358979323846;

enum axis
{
    X = 0,
    Y = 1
};

// attention! you cant get point (monitor_width, monitor_height)
// it is over bound
const int monitor_width = GetSystemMetrics(SM_CXSCREEN);
const int monitor_height = GetSystemMetrics(SM_CYSCREEN);

int px2pos(int px, axis d)
{
    if (d == X)
        return MulDiv(px, 65535, monitor_width - 1);
    else
        return MulDiv(px, 65535, monitor_height - 1);
}

int pos2px(int pos, axis d)
{
    if (d == X)
        return MulDiv(monitor_width - 1, pos, 65535);
    else
        return MulDiv(monitor_height - 1, pos, 65535);
}

//  (0, 0)-----------(65535, 0    )--x+
//  |   the screen is     |
//  |     divided into    |
//  |      65535*65535    |
//  (0, 65535)-------(65535, 65535)
//  y+

namespace mouse
{
    /// @brief translate cursor
    /// @param a the angle away from x+
    /// @param distant how many PXs will cursor translate
    void translate(angle a, int distant, unsigned int time_ms)
    {
        // get the cursor
        // cur.?'s unit: px
        POINT cur;
        GetCursorPos(&cur);
        // use Trigonometric funcs to calculate the dest
        // if cur = (x, y),
        // the dst will = ()
        // a may >= 360 || <= -360, so we need mod it
        // we cant % a double(angle), so deal with it by a loop
        if (a >= 360.0)
            while (a -= 360.0, a >= 360.0)
            {
            }
        if (a <= -360.0)
            while (a += 360.0, a <= -360.0)
            {
            }

        // dst?'s unit: px
        // convert angle from degrees to radians
        double rad = a * PI / 180.0;
        int dstx = cur.x + static_cast<int>(cos(rad) * distant);
        int dsty = cur.y + static_cast<int>(sin(rad) * distant);
        move_to(dstx, dsty, time_ms);
    }

    /// @brief translate cursor
    /// @param a the angle away from x+
    /// @param distant how many PXs will cursor translate
    void translate(angle a, int distant)
    {
        // get the cursor
        // cur.?'s unit: px
        POINT cur;
        GetCursorPos(&cur);
        // use Trigonometric funcs to calculate the dest
        // if cur = (x, y),
        // the dst will = ()
        // a may >= 360 || <= -360, so we need mod it
        // we cant % a double(angle), so deal with it by a loop
        if (a >= 360.0)
            while (a -= 360.0, a >= 360.0)
            {
            }
        if (a <= -360.0)
            while (a += 360.0, a <= -360.0)
            {
            }

        // dst?'s unit: px
        // convert angle from degrees to radians
        double rad = a * PI / 180.0;
        int dstx = cur.x + static_cast<int>(cos(rad) * distant);
        int dsty = cur.y + static_cast<int>(sin(rad) * distant);
        move_to(dstx, dsty);
    }

    /// @brief move the mouse to (x, y) (right = x+, down = y +)
    /// @param x: how many PXs away from left boarder
    /// @param y: how many PXs away from up boarder
    /// @param time_ms: time duration for the smooth movement, in ms
    void move_to(unsigned int x, unsigned int y, unsigned int time_ms)
    {
        // get current position
        POINT cur;
        GetCursorPos(&cur);
        int start_x = cur.x;
        int start_y = cur.y;

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

            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
            input.mi.dx = px2pos(cur_x, X);
            input.mi.dy = px2pos(cur_y, Y);
            SendInput(1, &input, sizeof(INPUT));

            Sleep(smoothmv_frametime);
        }
        // in case float deviation
        move_to(x, y);
    }

    /// @brief move the mouse to (x, y) (right = x+, down = y +)
    /// @param x: how many PXs away from left boarder
    /// @param y: how many PXs away from up boarder
    void move_to(unsigned int x, unsigned int y)
    {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        // sub 1, because px starts with 0
        input.mi.dx = px2pos(x, X);
        input.mi.dy = px2pos(y, Y);
        SendInput(1, &input, sizeof(INPUT));
    }

    /// @brief click the `btn`
    void click(mouse_btns btn)
    {
        INPUT inputs[2]{}; // 2 event in need
        inputs[0].type = INPUT_MOUSE;
        inputs[1].type = INPUT_MOUSE;
        DWORD down_flag{}, up_flag{};
        switch (btn)
        {
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
    void press(mouse_btns btn)
    {
        INPUT input{};
        input.type = INPUT_MOUSE;
        switch (btn)
        {
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
    void release(mouse_btns btn)
    {
        INPUT input{};
        input.type = INPUT_MOUSE;
        switch (btn)
        {
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
    void wheel(wheel_rotations rotation, double scale)
    {
        if (scale == 0.0)
            return;
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        int delta = static_cast<int>(WHEEL_DELTA * scale);
        input.mi.mouseData = (rotation == WU) ? delta : -delta;
        SendInput(1, &input, sizeof(INPUT));
    }

}

namespace keyboard
{
    // map VK_* to keys for GLFW‑style codes (≥256)
    static keys vk2keys(int vk)
    {
        // alphanumeric & punctuation — same as ASCII / VK
        if (vk >= 32 && vk <= 122)
            return static_cast<keys>(vk);

        switch (vk)
        {
        case VK_ESCAPE:
            return keys::ESCAPE;
        case VK_RETURN:
            return keys::ENTER;
        case VK_TAB:
            return keys::TAB;
        case VK_BACK:
            return keys::BACKSPACE;
        case VK_INSERT:
            return keys::INSERT;
        case VK_DELETE:
            return keys::DELETE;
        case VK_RIGHT:
            return keys::RIGHT;
        case VK_LEFT:
            return keys::LEFT;
        case VK_DOWN:
            return keys::DOWN;
        case VK_UP:
            return keys::UP;
        case VK_PRIOR:
            return keys::PAGE_UP;
        case VK_NEXT:
            return keys::PAGE_DOWN;
        case VK_HOME:
            return keys::HOME;
        case VK_END:
            return keys::END;
        case VK_CAPITAL:
            return keys::CAPS_LOCK;
        case VK_SCROLL:
            return keys::SCROLL_LOCK;
        case VK_NUMLOCK:
            return keys::NUM_LOCK;
        case VK_SNAPSHOT:
            return keys::PRINT_SCREEN;
        case VK_PAUSE:
            return keys::PAUSE;
        case VK_LSHIFT:
            return keys::LEFT_SHIFT;
        case VK_LCONTROL:
            return keys::LEFT_CONTROL;
        case VK_LMENU:
            return keys::LEFT_ALT;
        case VK_LWIN:
            return keys::LEFT_SUPER;
        case VK_RSHIFT:
            return keys::RIGHT_SHIFT;
        case VK_RCONTROL:
            return keys::RIGHT_CONTROL;
        case VK_RMENU:
            return keys::RIGHT_ALT;
        case VK_RWIN:
            return keys::RIGHT_SUPER;
        case VK_APPS:
            return keys::KB_MENU;
        default:
            break;
        }

        // F1 – F12: VK_F1 = 0x70, your F1 = 290
        if (vk >= VK_F1 && vk <= VK_F12)
            return static_cast<keys>(290 + (vk - VK_F1));

        // numpad 0–9: VK_NUMPAD0 = 0x60, your KP_0 = 320
        if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
            return static_cast<keys>(320 + (vk - VK_NUMPAD0));

        switch (vk)
        {
        case VK_DECIMAL:
            return keys::KP_DECIMAL;
        case VK_DIVIDE:
            return keys::KP_DIVIDE;
        case VK_MULTIPLY:
            return keys::KP_MULTIPLY;
        case VK_SUBTRACT:
            return keys::KP_SUBTRACT;
        case VK_ADD:
            return keys::KP_ADD;
        default:
            break;
        }

        return keys::NULL; // unmapped
    }

    keys get_key_pressed()
    {
        for (int vk = 0x01; vk <= 0xFE; ++vk)
        {
            // why windows dont support get key?
            // fking ...
            if (!(GetAsyncKeyState(vk) & 0x8000))
                continue;
            keys k = vk2keys(vk);
            if (k != keys::NULL)
                return k;
        }
        return keys::NULL;
    }

    bool is_key_pressed(keys key)
    {
        int vk;
        int val = static_cast<int>(key);

        // letters / digits / punctuation — same as VK_*
        if (val > 0 && val < 256)
        {
            vk = val;
        }
        else
        {
            // reverse map: keys to VK_*
            static const struct
            {
                keys k;
                int vk;
            } table[] = {
                {keys::ESCAPE, VK_ESCAPE},
                {keys::ENTER, VK_RETURN},
                {keys::TAB, VK_TAB},
                {keys::BACKSPACE, VK_BACK},
                {keys::INSERT, VK_INSERT},
                {keys::DELETE, VK_DELETE},
                {keys::RIGHT, VK_RIGHT},
                {keys::LEFT, VK_LEFT},
                {keys::DOWN, VK_DOWN},
                {keys::UP, VK_UP},
                {keys::PAGE_UP, VK_PRIOR},
                {keys::PAGE_DOWN, VK_NEXT},
                {keys::HOME, VK_HOME},
                {keys::END, VK_END},
                {keys::CAPS_LOCK, VK_CAPITAL},
                {keys::SCROLL_LOCK, VK_SCROLL},
                {keys::NUM_LOCK, VK_NUMLOCK},
                {keys::PRINT_SCREEN, VK_SNAPSHOT},
                {keys::PAUSE, VK_PAUSE},
                {keys::LEFT_SHIFT, VK_LSHIFT},
                {keys::LEFT_CONTROL, VK_LCONTROL},
                {keys::LEFT_ALT, VK_LMENU},
                {keys::LEFT_SUPER, VK_LWIN},
                {keys::RIGHT_SHIFT, VK_RSHIFT},
                {keys::RIGHT_CONTROL, VK_RCONTROL},
                {keys::RIGHT_ALT, VK_RMENU},
                {keys::RIGHT_SUPER, VK_RWIN},
                {keys::KB_MENU, VK_APPS},
            };

            vk = 0;
            for (auto &e : table)
            {
                if (e.k == key)
                {
                    vk = e.vk;
                    break;
                }
            }

            // F1 – F12
            if (!vk && val >= 290 && val <= 301)
                vk = VK_F1 + (val - 290);

            // Numpad 0–9
            if (!vk && val >= 320 && val <= 329)
                vk = VK_NUMPAD0 + (val - 320);

            switch (val)
            {
            case 330:
                vk = VK_DECIMAL;
                break;
            case 331:
                vk = VK_DIVIDE;
                break;
            case 332:
                vk = VK_MULTIPLY;
                break;
            case 333:
                vk = VK_SUBTRACT;
                break;
            case 334:
                vk = VK_ADD;
                break;
            case 335:
                vk = VK_RETURN;
                break; // KP_ENTER
            }
        }

        return vk ? (GetAsyncKeyState(vk) & 0x8000) != 0 : false;
    }
}

// re-define in case mysterious bug
#define DELETE (0x00010000L)
#define NULL (void *)0
