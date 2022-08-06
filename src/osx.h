#pragma once

struct OSX_App_Impl;

struct Platform_App
{
    OSX_App_Impl* impl;
};

Platform_App platform_create_app();
void platform_destroy_app(Platform_App platform_app);

struct OSX_Window_Impl;

struct Platform_Window
{
    OSX_Window_Impl* impl;
};

Platform_Window platform_create_window(Platform_App app);
bool platform_window_closing(Platform_Window window);
void platform_destroy_window(Platform_Window window);
void* platform_window_get_raw_handle(Platform_Window window);

void platform_pump_events(Platform_App app, Platform_Window main_window);

bool platform_get_exe_path(Path* path);

bool message_box_yes_no(char const* title, char const* message);