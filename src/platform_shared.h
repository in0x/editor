#pragma once

enum File_Mode
{
    None = 0,
    Read = 0x01,
};

enum Input_Key_Code
{
    A,
    D,
    S,
    W,
    ESC,
    L_SHIFT,
    R_SHIFT,
    L_CTRL,
    R_CTRL,
    L_ALT,
    R_ALT,
    L_CMD,
    R_CMD,
    CAPSLOCK,
    // ...
    Key_Unmapped,
    Enum_Count,
};

struct Input_State
{
    bool key_down[Input_Key_Code::Enum_Count];
};