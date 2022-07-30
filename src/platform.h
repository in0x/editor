#pragma once

#include "core.h"

#if PLATFORM_WIN32
    #include "win32.h"
#elif PLATFORM_OSX
    #include "win32.h" // FIX(): Temporarily include Win32 on OSX until we fix up a common platform interface.
    #include "osx.h"
#else
    static_assert(false, "Unknown Platform");
#endif
