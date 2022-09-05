#pragma once
#include "num_types.h"
#include "stdarg.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#if defined(WIN32)
	#define PLATFORM_WIN32 1
	#define PLATFORM_OSX 0
#elif defined(__APPLE__)
	#define PLATFORM_WIN32 0
	#define PLATFORM_OSX 1
#endif

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

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))

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

enum class Print_Flags : u8
{
	NONE = 0,
	APPEND_NEWLINE = 0x1,
};

// NOTE(): Returned pointer must be deleted by caller.
char const* inplace_printf(char const* fmt, Print_Flags flags, va_list args);
char const* va_inplace_printf(char const* fmt, Print_Flags flags, ...);

bool handle_assert(char const* condition, char const* msg, ...);

void debug_break()
{
	#if PLATFORM_WIN32
		__debugbreak();
	#elif PLATFORM_OSX
		__builtin_debugtrap();
	#endif
}

#if PLATFORM_WIN32
#define ASSERT_MSG(condition, msg, ...) if ((condition) == false)  \
			if (handle_assert( #condition , msg, __VA_ARGS__ ))    \
				debug_break();								       \
    
#elif PLATFORM_OSX

#define ASSERT_MSG(condition, msg, ...) if ((condition) == false)  \
			if (handle_assert( #condition , msg, ##__VA_ARGS__ ))  \
				debug_break();								       \

#endif

#define ASSERT(condition) if ((condition) == false)   \
			if (handle_assert( #condition, nullptr )) \
				debug_break();						  \

constexpr bool C_ALWAYS_FAILS = false;

#define ASSERT_FAILED_MSG(msg, ...) ASSERT_MSG(C_ALWAYS_FAILS, msg, ## __VA_ARGS__)

void log_message(char const* fmt, ...);

#define LOG(fmt, ...) log_message(fmt, ## __VA_ARGS__);

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
void option_set(Option<T>* option, T value)
{
    option->value     = value;
    option->has_value = true;
}

struct String
{
	char* buffer = nullptr;
	u32 len = 0;
};

String alloc_string(char const* src);
void free_string(String str);