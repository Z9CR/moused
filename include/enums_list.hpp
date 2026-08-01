#pragma once

// X-Macros to generate enum
// because if we builtin them, it's hard to pass the param to lua side

#define MOUSE_BTN_LIST(X) \
    X(LMB, 0) /* MB1 in windows.h */ \
    X(RMB, 1) /* MB2 in windows.h */ \
    X(MMB, 2) /* MB3 in windows.h */ \
    X(XB1, 3) /* MB4 in windows.h */ \
    X(XB2, 4) /* MB5 in windows.h */


#define WHEEL_ROTATION_LIST(X) \
    X(WU, 0) /* WheelUp   */ \
    X(WD, 1) /* WheelDown */