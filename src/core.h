#pragma once
#include "stdarg.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

using s8 = int8_t;
using u8 = uint8_t;
using s16 = int16_t;
using u16 = uint16_t;
using s32 = int32_t;
using u32 = uint32_t;
using s64 = int64_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

#if defined(WIN32)
#define PLATFORM_WIN32 1
#define PLATFORM_OSX 0
#elif defined(__APPLE__)
#define PLATFORM_WIN32 0
#define PLATFORM_OSX 1
#endif

#if PLATFORM_WIN32
#define FORCEINLINE __forceinline
#elif PLATFORM_OSX
#define FORCEINLINE __attribute__((always_inline))
#endif

#define UNUSED_VAR(x) (void)x

#ifdef _DEBUG
#define DEBUG_BUILD 1
#else
#define DEBUG_BUILD 0
#endif

#define CONCAT_INNER(l, r) l##r
#define CONCAT(l, r) CONCAT_INNER(l, r)

#define UNIQUE_ID(x) CONCAT(x, __COUNTER__)

#define STRINGIFY_INNER(x) #x
#define STRINGIFY(x) STRINGIFY_INNER(x)

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))

template <typename T>
struct DeferredFunction
{
    DeferredFunction(T&& closure)
        : closure(static_cast<T&&>(closure))
    {
    }

    ~DeferredFunction()
    {
        closure();
    }

    T closure;
};

#define DEFER DeferredFunction const UNIQUE_ID(_scope_exit) = [&]

void handle_assert(char const* condition, char const* msg, ...);

#define __LOCATION_INFO__ "In: " __FILE__ "\nAt: " STRINGIFY(__LINE__) ", " __FUNCTION__ "() "

#if PLATFORM_WIN32
#define ASSERT_MSG(condition, msg, ...) \
    if (bool(condition) == false)           \
    handle_assert(#condition, msg, __VA_ARGS__)

#elif PLATFORM_OSX

#define ASSERT_MSG(condition, msg, ...) \
    if (bool(condition) == false)           \
    handle_assert(#condition, msg, ##__VA_ARGS__)

#endif

#define ASSERT(condition)     \
    if (bool(condition) == false) \
    handle_assert(#condition, nullptr)

constexpr bool C_ALWAYS_FAILS = false;

#define ASSERT_FAILED_MSG(msg, ...) ASSERT_MSG(C_ALWAYS_FAILS, msg, ##__VA_ARGS__)

void log_message(char const* fmt, ...);

#define LOG(fmt, ...) log_message(fmt, ##__VA_ARGS__);

void log_last_platform_error();

#if PLATFORM_OSX
#define MAX_PATH 260
#endif

template <typename T>
struct Option
{
    T value = {};
    bool has_value = false;
};

template <typename T>
static T const& operator*(Option<T> const& opt)
{
    ASSERT(opt.has_value);
    return opt.value;
}

template <typename T>
void option_set(Option<T>* option, T value)
{
    option->value = value;
    option->has_value = true;
}

struct String
{
    char* buffer = nullptr;
    u32 len = 0;
};

void mem_zero(void* dst, u64 len);

template <typename T>
void zero_struct(T* p_struct)
{
    mem_zero(p_struct, sizeof(T));
}