#pragma once
#include "stdarg.h"
#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"

#include "win32.h"

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

#define UNUSED_VAR(x) (void)x

#ifdef _DEBUG
#define DEBUG_BUILD 1
#else
#define DEBUG_BUILD 0
#endif

#define CONCAT_INNER(l, r) l ## r
#define CONCAT(l, r) CONCAT_INNER(l, r)

#define UNIQUE_ID(x) CONCAT(x, __COUNTER__)

#define STRINGIFY_INNER(x) #x
#define STRINGIFY(x) STRINGIFY_INNER(x)

template <typename T>
struct DeferredFunction
{
	DeferredFunction(T&& closure)
		: closure(static_cast<T&&>(closure))
	{}

	~DeferredFunction()
	{
		closure();
	}

	T closure;
};

#define DEFER DeferredFunction const UNIQUE_ID(_scope_exit) = [&] 

enum Print_Flags
{
	NONE = 0,
	APPEND_NEWLINE = 0x1,
};

// NOTE(): Returned pointer must be deleted by caller.
char const* inplace_printf(char const* fmt, Print_Flags flags, va_list args);
char const* va_inplace_printf(char const* fmt, Print_Flags flags, ...);

bool handle_assert(char const* condition, char const* msg, ...);

#define ASSERT_MSG(condition, msg, ...) if ((condition) == false)  \
			if (handle_assert( #condition , msg, __VA_ARGS__)) \
				__debugbreak();								   \

#define ASSERT(condition) if ((condition) == false)   \
			if (handle_assert( #condition, nullptr )) \
				__debugbreak();						  \

constexpr bool C_ALWAYS_FAILS = false;

#define ASSERT_FAILED_MSG(msg, ...) ASSERT_MSG(C_ALWAYS_FAILS, msg, __VA_ARGS__)

void log_message(char const* fmt, ...);

#define LOG(fmt, ...) log_message(fmt, __VA_ARGS__);

void log_last_windows_error();