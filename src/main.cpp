#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"

#ifndef NOMINMAX 
#define NOMINMAX 
#endif  

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using s8  =  int8_t;
using u8  = uint8_t;
using s16 =  int16_t;
using u16 = uint16_t;
using s32 =  int32_t;
using u32 = uint32_t;
using s64 =  int64_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

#define UNUSED_VAR(x) (void)x

enum printf_flags
{
	NONE = 0,
	APPEND_NEWLINE = 0x1,
};

// NOTE(): Returned pointer must be deleted by caller.
char const* inplace_printf(char const* fmt, printf_flags flags, va_list args)
{
	bool append_newline = flags & printf_flags::APPEND_NEWLINE;

	va_list args_copy;
	va_copy(args_copy, args);

	int required_buffer_size = vsnprintf(nullptr, 0, fmt, args);
		required_buffer_size += (append_newline) ? 2: 1;

	char* msg_buffer = new char[required_buffer_size];
	
	int write_result = vsnprintf_s(msg_buffer, required_buffer_size, _TRUNCATE, fmt, args_copy);
	UNUSED_VAR(write_result);
	va_end(args_copy);

	if (append_newline)
	{
		msg_buffer[required_buffer_size - 2] = '\n';
	}
	
	msg_buffer[required_buffer_size - 1] = '\0';

	return msg_buffer;
}

char const* va_inplace_printf(char const* fmt, printf_flags flags, ...)
{
	va_list args;
	va_start(args, flags);
	char const* result = inplace_printf(fmt, flags, args);
	va_end(args);

	return result;
}

bool handle_assert(char const* condition, char const* msg, ...)
{
	va_list user_args;
	va_start(user_args, msg);
	char const* user_msg = inplace_printf(msg, printf_flags::NONE, user_args);
	va_end(user_args);

	char const* assert_msg = va_inplace_printf("Condition: %s\nMessage: %s", printf_flags::APPEND_NEWLINE, condition, user_msg);

	bool should_break = (IDYES == MessageBoxA(NULL, assert_msg, "Assert Failed! Break into code?", MB_YESNO | MB_ICONERROR));

	delete user_msg;
	delete assert_msg;

	return should_break;
}

#define ASSERT(condition, msg, ...) if ((condition) == false)  \
			if (handle_assert( #condition , msg, __VA_ARGS__)) \
				__debugbreak();								   \

void log_message(char const* fmt, ...) // TODO: append newline
{
	va_list args;
	va_start(args, fmt);
	
	// TODO: we dont want to allocate on every log, so reserve a logging buffer instead.
	// will need to update inplace_printf to handle truncation (if not write to end of write, cant assume full buffer is used)
	char const* msg = inplace_printf(fmt, printf_flags::APPEND_NEWLINE, args);
	OutputDebugStringA(msg);

	delete msg;
	va_end(args);
}

#define LOG(fmt, ...) log_message(fmt, __VA_ARGS__);

void log_last_windows_error()
{
	u32 last_error = GetLastError();
	if (last_error == ERROR_SUCCESS)
	{
		return;
	}

	LPTSTR error_text = nullptr;
	DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;

	FormatMessage(
		flags,
		nullptr, // unused with FORMAT_MESSAGE_FROM_SYSTEM
		last_error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&error_text,
		0,
		nullptr);

	if (error_text != nullptr)
	{
		LOG("%ls", error_text);
		LocalFree(error_text);
	}
	else
	{
		LOG("Failed to get message for last windows error %u", static_cast<u32>(last_error));
	}
}

static LRESULT CALLBACK OnMainWindowEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(handle, message, wParam, lParam);
}

INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) 
{
    {
		ATOM class_handle = INVALID_ATOM;
		WCHAR const* window_class_name = L"editor_window_class";

		{
			WNDCLASSEX window_class;
			memset(&window_class, 0, sizeof(WNDCLASSEX));
			window_class.cbSize = sizeof(WNDCLASSEX);
			window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
			window_class.lpfnWndProc = &OnMainWindowEvent;
			window_class.hInstance = GetModuleHandle(nullptr);
			window_class.lpszClassName = window_class_name;

			class_handle = RegisterClassEx(&window_class);
			
			ASSERT(class_handle != INVALID_ATOM, "Failed to register window class type %ls!\n", window_class_name);

		}
		
		if (class_handle == INVALID_ATOM)
		{
			log_last_windows_error();
		}

    }

    u32 window_width = 800;
    u32 window_height = 600;

	LOG("Hello Editor"); // TODO(why do we end up using a double new-line to get the right output?
}