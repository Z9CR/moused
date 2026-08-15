#pragma once

// X-Macros to generate enum
// (single source of truth for names & values used across platforms)

#pragma region MOUSE_BTN_LIST
#define MOUSE_BTN_LIST(ITEM)            \
    ITEM(LMB, 0) /* MB1 in windows.h */ \
    ITEM(RMB, 1) /* MB2 in windows.h */ \
    ITEM(MMB, 2) /* MB3 in windows.h */ \
    ITEM(XB1, 3) /* MB4 in windows.h */ \
    ITEM(XB2, 4) /* MB5 in windows.h */
#pragma endregion

#pragma region WHEEL_ROTATION_LIST
#define WHEEL_ROTATION_LIST(ITEM) \
    ITEM(WU, 0) /* WheelUp   */   \
    ITEM(WD, 1) /* WheelDown */
#pragma endregion

#pragma region KEYS_LIST
#define KEYS_LIST(ITEM)      \
    ITEM(NONE, 0)            \
    ITEM(APOSTROPHE, 39)     \
    ITEM(COMMA, 44)          \
    ITEM(MINUS, 45)          \
    ITEM(PERIOD, 46)         \
    ITEM(SLASH, 47)          \
    ITEM(ZERO, 48)           \
    ITEM(ONE, 49)            \
    ITEM(TWO, 50)            \
    ITEM(THREE, 51)          \
    ITEM(FOUR, 52)           \
    ITEM(FIVE, 53)           \
    ITEM(SIX, 54)            \
    ITEM(SEVEN, 55)          \
    ITEM(EIGHT, 56)          \
    ITEM(NINE, 57)           \
    ITEM(SEMICOLON, 59)      \
    ITEM(EQUAL, 61)          \
    ITEM(A, 65)              \
    ITEM(B, 66)              \
    ITEM(C, 67)              \
    ITEM(D, 68)              \
    ITEM(E, 69)              \
    ITEM(F, 70)              \
    ITEM(G, 71)              \
    ITEM(H, 72)              \
    ITEM(I, 73)              \
    ITEM(J, 74)              \
    ITEM(K, 75)              \
    ITEM(L, 76)              \
    ITEM(M, 77)              \
    ITEM(N, 78)              \
    ITEM(O, 79)              \
    ITEM(P, 80)              \
    ITEM(Q, 81)              \
    ITEM(R, 82)              \
    ITEM(S, 83)              \
    ITEM(T, 84)              \
    ITEM(U, 85)              \
    ITEM(V, 86)              \
    ITEM(W, 87)              \
    ITEM(X, 88)              \
    ITEM(Y, 89)              \
    ITEM(Z, 90)              \
    ITEM(LEFT_BRACKET, 91)   \
    ITEM(BACKSLASH, 92)      \
    ITEM(RIGHT_BRACKET, 93)  \
    ITEM(GRAVE, 96)          \
    ITEM(SPACE, 32)          \
    ITEM(ESCAPE, 256)        \
    ITEM(ENTER, 257)         \
    ITEM(TAB, 258)           \
    ITEM(BACKSPACE, 259)     \
    ITEM(INSERT, 260)        \
    ITEM(DELETE, 261)        \
    ITEM(RIGHT, 262)         \
    ITEM(LEFT, 263)          \
    ITEM(DOWN, 264)          \
    ITEM(UP, 265)            \
    ITEM(PAGE_UP, 266)       \
    ITEM(PAGE_DOWN, 267)     \
    ITEM(HOME, 268)          \
    ITEM(END, 269)           \
    ITEM(CAPS_LOCK, 280)     \
    ITEM(SCROLL_LOCK, 281)   \
    ITEM(NUM_LOCK, 282)      \
    ITEM(PRINT_SCREEN, 283)  \
    ITEM(PAUSE, 284)         \
    ITEM(F1, 290)            \
    ITEM(F2, 291)            \
    ITEM(F3, 292)            \
    ITEM(F4, 293)            \
    ITEM(F5, 294)            \
    ITEM(F6, 295)            \
    ITEM(F7, 296)            \
    ITEM(F8, 297)            \
    ITEM(F9, 298)            \
    ITEM(F10, 299)           \
    ITEM(F11, 300)           \
    ITEM(F12, 301)           \
    ITEM(LEFT_SHIFT, 340)    \
    ITEM(LEFT_CONTROL, 341)  \
    ITEM(LEFT_ALT, 342)      \
    ITEM(LEFT_SUPER, 343)    \
    ITEM(RIGHT_SHIFT, 344)   \
    ITEM(RIGHT_CONTROL, 345) \
    ITEM(RIGHT_ALT, 346)     \
    ITEM(RIGHT_SUPER, 347)   \
    ITEM(KB_MENU, 348)       \
    ITEM(KP_0, 320)          \
    ITEM(KP_1, 321)          \
    ITEM(KP_2, 322)          \
    ITEM(KP_3, 323)          \
    ITEM(KP_4, 324)          \
    ITEM(KP_5, 325)          \
    ITEM(KP_6, 326)          \
    ITEM(KP_7, 327)          \
    ITEM(KP_8, 328)          \
    ITEM(KP_9, 329)          \
    ITEM(KP_DECIMAL, 330)    \
    ITEM(KP_DIVIDE, 331)     \
    ITEM(KP_MULTIPLY, 332)   \
    ITEM(KP_SUBTRACT, 333)   \
    ITEM(KP_ADD, 334)        \
    ITEM(KP_ENTER, 335)      \
    ITEM(KP_EQUAL, 336)
#pragma endregion