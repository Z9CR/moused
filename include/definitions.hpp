#pragma once

constexpr enum direction {
    // Raylib coordinate sys
    left = 0, // x-
    right = 1,// x+
    up = 2,   // y-
    down = 3  // y+
};

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
