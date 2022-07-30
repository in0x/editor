#pragma once

struct OSX_App_Impl;

struct OSX_App
{
    OSX_App_Impl* impl;
};

OSX_App osx_create_app();
void osx_destroy_app(OSX_App app);

struct OSX_Window_Impl;

struct OSX_Window
{
    OSX_Window_Impl* impl;
};

OSX_Window osx_create_window(OSX_App app);
bool osx_window_closing(OSX_Window window);
void osx_destroy_window(OSX_Window window);

void osx_pump_events();

void osx_message_box_yes_no();