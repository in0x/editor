#include "core.h"
#include "platform.h"

void debug_break()
{
#if PLATFORM_WIN32
    __debugbreak();
#elif PLATFORM_OSX
    __builtin_debugtrap();
#endif
}

bool debug_break_if_attached()
{
    if (platform_is_debugger_present())
    {
        debug_break();
        return true;
    }
    else
    {
        return false;
    }
}

enum class Print_Flags : u8
{
    NONE = 0,
    APPEND_NEWLINE = 0x1,
};

static bool is_set(Print_Flags flags, Print_Flags val)
{
    return u32(flags) & u32(val);
}

static void inplace_printf(char const* fmt, char* buf, u64 buf_len, Print_Flags flags, va_list args)
{
    bool append_newline = is_set(flags, Print_Flags::APPEND_NEWLINE);

    va_list args_copy;
    va_copy(args_copy, args);

    int required_buffer_size = vsnprintf(nullptr, 0, fmt, args);
    required_buffer_size += (append_newline) ? 2 : 1;

    if (required_buffer_size > buf_len)
    {
        printf("Found log thats longer (%d) than the provided target buffer (%llu), truncating the log.\n",
               required_buffer_size, buf_len);
        debug_break_if_attached();
        required_buffer_size = buf_len;
    }

#if PLATFORM_WIN32
    int write_result = vsnprintf_s(buf, required_buffer_size, _TRUNCATE, fmt, args_copy);
#else
    int write_result = vsnprintf(buf, required_buffer_size, fmt, args_copy);
#endif

    UNUSED_VAR(write_result);
    va_end(args_copy);

    if (append_newline)
    {
        buf[required_buffer_size - 2] = '\n';
    }

    buf[required_buffer_size - 1] = '\0';
}

static void va_inplace_printf(char const* fmt, char* buf, u64 buf_len, Print_Flags flags, ...)
{
    va_list args;
    va_start(args, flags);
    inplace_printf(fmt, buf, buf_len, flags, args);
    va_end(args);
}

constexpr u64 c_msg_buffer_size = 800;
constexpr u64 c_log_buffer_size = 1000;

static thread_local char tls_msg_buffer[c_msg_buffer_size];
static thread_local char tls_log_buffer[c_log_buffer_size];

void handle_assert(char const* condition, char const* msg, ...)
{
    // TODO(): Assert handling should be thread-safe and should block the frame end-point.

    if (msg)
    {
        va_list user_args;
        va_start(user_args, msg);
        inplace_printf(msg, tls_msg_buffer, c_msg_buffer_size, Print_Flags::NONE, user_args);
        va_end(user_args);

        va_inplace_printf("Condition: %s\nMessage: %s",
                          tls_log_buffer, c_log_buffer_size, Print_Flags::APPEND_NEWLINE, condition, tls_msg_buffer);
    }
    else
    {
        va_inplace_printf("Condition: %s",
                          tls_log_buffer, c_log_buffer_size, Print_Flags::APPEND_NEWLINE, condition);
    }

    printf("ASSERT HIT:\n%s", tls_log_buffer);

    if (platform_is_debugger_present())
    {
        debug_break();
    }
    else if (message_box_yes_no("Assert Failed! Break into debugger?", tls_log_buffer))
    {
        debug_break();
    }
}

void log_message(char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    inplace_printf(fmt, tls_log_buffer, c_log_buffer_size, Print_Flags::APPEND_NEWLINE, args);
    printf("%s", tls_log_buffer);

    va_end(args);
}

void log_last_platform_error()
{
#if PLATFORM_WIN32
    Win32Error err = get_last_windows_error();
    if (err.msg && err.is_error())
    {
        LOG("%ls", err.msg);
        free_win32_error(err);
    }
#endif
}