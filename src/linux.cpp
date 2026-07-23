#include <adapter.hpp>
#include <linux.hpp>
#include <definitions.hpp>


/// @brief translate cursor
/// @param a the angle away from x-
/// @param distant how many PXs will cursor translate
void mouse::translate(angle a, int distant, unsigned int time_ms) {
    
}

/// @brief translate cursor
/// @param a the angle away from x-
/// @param distant how many PXs will cursor translate
void mouse::translate(angle a, int distant) {
    
}

/// @brief move the mouse to (x, y) (right = x+, down = y +)
/// @param x: how many PXs away from left boarder
/// @param y: how many PXs away from up boarder
void mouse::move_to(unsigned int x, unsigned int y, unsigned int time_ms) {
    
}

/// @brief move the mouse to (x, y) (right = x+, down = y +)
/// @param x: how many PXs away from left boarder
/// @param y: how many PXs away from up boarder
void mouse::move_to(unsigned int x, unsigned int y) {

}

/// @brief click the `btn`
void mouse::click(mouse_btns btn) {

}

/// @brief press the `btn`
void mouse::press(mouse_btns btn) {
    
}

/// @brief release the pressed `btn`
void mouse::release(mouse_btns btn) {

}

/// @brief routate the MMB for `scale`*Delta
void mouse::wheel(wheel_rotations rotation, double scale) {
    
}