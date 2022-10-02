#pragma once

#include "core.h"

#if PLATFORM_WIN32
#include "win32.h"
#elif PLATFORM_OSX
#include "osx.h"
#define platform_message
#endif
