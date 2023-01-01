#pragma once

struct Create_Window_Params
{
    s32 x = 0;
    s32 y = 0;
    s32 width = 0;
    s32 height = 0;
    char const* title = nullptr;
};

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