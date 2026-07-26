/*
 * declare funcs here, impl them in diff .cpp for cross platform
 */
#pragma once

/* if angle = 0, 
* it will point to x+ direction
* which is the right direction in physical world
* if angle > 0,
* it will point to the direction CW rotate `angle` degs from x+
* if angle > 0,
* it will point to the direction CCW rotate `angle` degs from x+
*/
typedef double angle;

namespace mouse
{

    enum mouse_btns
    {
        LMB = 0, // MB1 in windows.h
        RMB = 1, // MB2 in windows.h
        MMB = 2, // MB3 in windows.h
        XB1 = 3, // MB4 in windows.h
        XB2 = 4  // MB5 in windows.h
    };

    enum wheel_rotations
    {
        WU = 0, // WheelUp
        WD = 1  // WheelDown
    };

    /// @brief translate cursor
    /// @param a the angle away from x-
    /// @param distant how many PXs will cursor translate
    void translate(angle a, int distant);

    /// @brief translate cursor
    /// @param a the angle away from x-
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

namespace keyboard
{
    enum spec_key
    {

    };

}