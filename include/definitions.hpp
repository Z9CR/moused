#pragma once

constexpr enum mouse_btns {
    LMB = 0, // MB1 in windows.h
    RMB = 1, // MB2 in windows.h
    MMB = 2, // MB3 in windows.h
    XB1 = 3, // MB4 in windows.h
    XB2 = 4  // MB5 in windows.h
};

constexpr enum wheel_rotations {
    WU = 0, // WheelUp
    WD = 1  // WheelDown
};

/// provide unsigned f64 macro to require dev provides a unsigned val
using uf64 = double;

/// if angel = 0, it points to x- direction(up in physical world)
/// if it > 0, it points to the direction CW rotated `angel` degrees from x- direction
/// if it < 0, it points to the direction CCW rotated `angel` degrees from x- direction
using angel = double;