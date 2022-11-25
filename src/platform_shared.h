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
    // ...
    Enum_Count,
};

struct Input_Events
{
    bool key_down[Input_Key_Code::Enum_Count];
};