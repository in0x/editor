#pragma once

#if PLATFORM_WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h> // TODO(): hide in platform impl file

static const WCHAR* c_win32_default_err_msg = L"Failed to get message for this windows error";
static const WCHAR* c_win32_success_msg = L"This windows operation completed succesfully";

static const LPWSTR c_p_win32_default_err_msg = const_cast<WCHAR*>(c_win32_default_err_msg);
static const LPWSTR c_p_win32_success_msg = const_cast<WCHAR*>(c_win32_success_msg);

struct Platform_App
{
};

Platform_App create_platform_app() {}
void destroy_platform_app(Platform_App platform_app) {}

struct Platform_Window
{
};

struct Win32Error
{
	DWORD code = 0;
	LPWSTR msg = nullptr;

	bool is_error()
	{
		return code != ERROR_SUCCESS;
	}
};

static void free_win32_error(Win32Error error)
{
	if ((error.msg != c_p_win32_default_err_msg) &&
		(error.msg != c_p_win32_success_msg))
	{
		LocalFree(error.msg);
	}
}

static Win32Error get_last_windows_error()
{
	Win32Error error;
	error.code = GetLastError();
	if (error.code == ERROR_SUCCESS)
	{
		error.msg = c_p_win32_success_msg;
		return error;
	}

	DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;

	FormatMessage(
		flags,
		nullptr, // unused with FORMAT_MESSAGE_FROM_SYSTEM
		error.code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		error.msg,
		0,
		nullptr);

	if (error.msg == nullptr)
	{
		error.msg = c_p_win32_default_err_msg;
	}

	return error;
}

// void message_box_yes_no(); // TODO()

static LRESULT CALLBACK OnMainWindowEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
		SetWindowLongPtr(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
		break;
	}

	case WM_CLOSE:
	{
		PostQuitMessage(0);
		break;
	}
	}

	return DefWindowProcW(handle, message, wParam, lParam);
}

struct Create_Window_Params
{
    u32 x;
    u32 y;
    u32 width;
    u32 height;
    WCHAR const* class_name;
    WCHAR const* title;
};

static HWND create_window(Create_Window_Params* params)
{
    WNDCLASSEX window_class = {};
    window_class.cbSize = sizeof(WNDCLASSEX);
    window_class.style =  CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc =   &OnMainWindowEvent;
    window_class.hInstance =     GetModuleHandle(nullptr);
    window_class.lpszClassName = params->class_name;

    ATOM class_handle = RegisterClassEx(&window_class);
    ASSERT_MSG(class_handle != INVALID_ATOM, "Failed to register window class type %ls!", params->class_name);

    if (class_handle == INVALID_ATOM)
    {
        log_last_platform_error();
        return (HWND)INVALID_HANDLE_VALUE;
    }

    RECT window_dim = {};
    window_dim.left = params->x;
    window_dim.top =  params->y;
    window_dim.bottom = params->y + params->height;
    window_dim.right =  params->x + params->width;

    DWORD ex_window_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
    DWORD window_style = WS_OVERLAPPEDWINDOW;

    BOOL has_menu = FALSE;
    AdjustWindowRect(&window_dim, WS_OVERLAPPEDWINDOW, has_menu);

    WCHAR const* window_title = L"Editor";

    HANDLE window_handle = CreateWindowEx(
                            ex_window_style,
                            params->class_name,
                            params->title,
                            WS_CLIPSIBLINGS | WS_CLIPCHILDREN | window_style,
                            window_dim.left,
                            window_dim.top,
                            window_dim.right - window_dim.left,
                            window_dim.bottom - window_dim.top,
                            nullptr,
                            nullptr,
                            GetModuleHandle(nullptr),
                            nullptr);

    if (window_handle != INVALID_HANDLE_VALUE)
    {
        LOG("Created new window TITLE: %ls X: %ld Y: %ld WIDTH: %ld HEIGHT: %ld", params->title, window_dim.left, window_dim.top, 
            window_dim.right - window_dim.left, window_dim.bottom - window_dim.top);
    }
    else
    {
        log_last_platform_error();
        ASSERT_MSG(window_handle != INVALID_HANDLE_VALUE, "Failed to create main window!");
    }

    return (HWND)window_handle;
}

static void close_window(HWND handle, Create_Window_Params* params)
{
    UnregisterClass(params->class_name, GetModuleHandle(nullptr));
}

#else // TODO(): We'll want some common interface once we implement
	  // similar features for OSX, but for now just define it empty.

struct Win32Error {};

static void free_win32_error(Win32Error error) {}
static Win32Error get_last_windows_error() { return {}; }

void platform_pump_events(Platform_App app, Platform_Window main_window)
{
    MSG msg = {};

    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (WM_QUIT == msg.message)
        {
            app.exit_app = true;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}


bool platform_get_exe_path(Path* path)
{
    s32 retval = GetModuleFileNameA(nullptr, path.buffer, path.buffer_len());
    return retval > 0;
}

#endif // PLATFORM_WIN32