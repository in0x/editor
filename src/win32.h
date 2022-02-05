#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h> // TODO(): hide in platform impl file

static const WCHAR* c_win32_default_err_msg = L"Failed to get message for this windows error";
static const WCHAR* c_win32_success_msg = L"This windows operation completed succesfully";

static const LPWSTR c_p_win32_default_err_msg = const_cast<WCHAR*>(c_win32_default_err_msg);
static const LPWSTR c_p_win32_success_msg = const_cast<WCHAR*>(c_win32_success_msg);

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