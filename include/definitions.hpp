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

/* if angle = 0, 
* it will point to x+ direction
* which is the right direction in physical world
* if angle > 0,
* it will point to the direction CW rotate `angle` degs from x+
* if angle > 0,
* it will point to the direction CCW rotate `angle` degs from x+
*/
using angle = double;