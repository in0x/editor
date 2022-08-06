#pragma once

#include "core.h"

// define platform common code and structures here so the included files can see them

#if PLATFORM_OSX
    #define MAX_PATH 260
#endif

struct Path
{
    char buffer[MAX_PATH] = "\0";
    constexpr u64 buffer_len() const { return MAX_PATH; }
};

#if PLATFORM_WIN32
    #include "win32.h"
#elif PLATFORM_OSX
    #include "osx.h"
    #define platform_message
#endif
