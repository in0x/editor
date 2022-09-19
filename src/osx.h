#pragma once

#include "core.h"
#include "array.h"
#include "stdio.h"

bool platform_is_debugger_present();

struct String;
struct OSX_Window_Impl;
struct OSX_App_Impl;

struct Platform_App
{
    OSX_App_Impl* impl;
};

Platform_App platform_create_app();
void platform_destroy_app(Platform_App platform_app);

struct Platform_Window
{
    OSX_Window_Impl* impl;
};

struct Create_Window_Params
{
    u32 x;
    u32 y;
    u32 width;
    u32 height;
};

Platform_Window platform_create_window(Platform_App app, Create_Window_Params params);
bool platform_window_closing(Platform_Window window);
void platform_destroy_window(Platform_Window window);
void* platform_window_get_raw_handle(Platform_Window window);

void platform_pump_events(Platform_App app, Platform_Window main_window);

bool platform_get_exe_path(String* path);

bool message_box_yes_no(char const* title, char const* message);

struct File_Handle
{
    FILE* handle = nullptr;
    String path;
};

bool is_file_valid(File_Handle handle);
File_Handle open_file(String path);
void close_file(File_Handle file);
Option<u64> get_file_size(File_Handle file);
Option<u64> read_file(File_Handle file, Array<u8>* buffer, u32 num_bytes);
