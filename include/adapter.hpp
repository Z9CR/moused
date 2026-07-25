/*
 * declare funcs here, impl them in diff .cpp for cross platform
 */
#pragma once
#include <definitions.hpp>

namespace mouse
{
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