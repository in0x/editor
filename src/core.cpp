#include "core.h"
#include "platform.h"

bool is_set(Print_Flags flags, Print_Flags val)
{
    return u32(flags) & u32(val);
}

char const* inplace_printf(char const* fmt, Print_Flags flags, va_list args)
{
	bool append_newline = is_set(flags, Print_Flags::APPEND_NEWLINE);

	va_list args_copy;
	va_copy(args_copy, args);

	int required_buffer_size = vsnprintf(nullptr, 0, fmt, args);
	required_buffer_size += (append_newline) ? 2 : 1;

	char* msg_buffer = new char[required_buffer_size];

#if PLATFORM_WIN32
	int write_result = vsnprintf_s(msg_buffer, required_buffer_size, _TRUNCATE, fmt, args_copy);
#else
	int write_result = vsnprintf(msg_buffer, required_buffer_size, fmt, args_copy);    
#endif

    UNUSED_VAR(write_result);
	va_end(args_copy);

	if (append_newline)
	{
		msg_buffer[required_buffer_size - 2] = '\n';
	}

	msg_buffer[required_buffer_size - 1] = '\0';

	return msg_buffer;
}

char const* va_inplace_printf(char const* fmt, Print_Flags flags, ...)
{
	va_list args;
	va_start(args, flags);
	char const* result = inplace_printf(fmt, flags, args);
	va_end(args);

	return result;
}

bool handle_assert(char const* condition, char const* msg, ...)
{
	// TODO(): Assert handling should be thread-safe and should block the frame end-point.

	char const* user_msg = nullptr;
	DEFER{ delete user_msg; };
	if (msg)
	{
		va_list user_args;
		va_start(user_args, msg);
		user_msg = inplace_printf(msg, Print_Flags::NONE, user_args);
		va_end(user_args);
	}

	char const* assert_msg = nullptr;
	DEFER{ delete assert_msg; };
	if (user_msg)
	{
		assert_msg = va_inplace_printf("Condition: %s\nMessage: %s", Print_Flags::APPEND_NEWLINE, condition, user_msg);
	}
	else
	{
		assert_msg = va_inplace_printf("Condition: %s", Print_Flags::APPEND_NEWLINE, condition);
	}

	LOG("ASSERT HIT:\n%s", assert_msg);

	// bool should_break = (IDYES == MessageBoxA(NULL, assert_msg, "Assert Failed! Break into code?", MB_YESNO | MB_ICONERROR));

    bool should_break = message_box_yes_no("Assert Failed! Break into code?", assert_msg);
    
	return should_break;
}

void log_message(char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	// TODO(): we dont want to allocate on every log, so reserve a logging buffer instead.
	// will need to update inplace_printf to handle truncation (if not write to end of write, cant assume full buffer is used)
	char const* msg = inplace_printf(fmt, Print_Flags::APPEND_NEWLINE, args);
	printf("%s", msg);

	delete msg;
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