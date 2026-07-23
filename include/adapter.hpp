/*
 * declare funcs here, impl them in diff .cpp for cross platform
 */
#pragma once
#include <definitions.hpp>

class mouse
{
public:
    /// @brief translate cursor
    /// @param a the angle away from x-
    /// @param distant how many PXs will cursor translate
    static void translate(angle a, int distant);

    /// @brief translate cursor
    /// @param a the angle away from x-
    /// @param distant how many PXs will cursor translate
    static void translate(angle a, int distant, unsigned int time_ms);

    /// @brief move the mouse to (x, y) (right = x+, down = y +)
    /// @param x: how many PXs away from left boarder
    /// @param y: how many PXs away from up boarder
    static void move_to(unsigned int x, unsigned int y, unsigned int time_ms);

    /// @brief move the mouse to (x, y) (right = x+, down = y +)
    /// @param x: how many PXs away from left boarder
    /// @param y: how many PXs away from up boarder
    static void move_to(unsigned int x, unsigned int y);

    /// @brief click the `btn`
    static void click(mouse_btns btn);

    /// @brief press the `btn`
    static void press(mouse_btns btn);

    /// @brief release the pressed `btn`
    static void release(mouse_btns btn);

    /// @brief routate the MMB for `scale`*Delta
    static void wheel(wheel_rotations rotation, double scale);
};