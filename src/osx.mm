#include "osx.h"

#include "core.h"
#include "memory.h"
#include "stdio.h"

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <os/log.h>

#include <mach-o/dyld.h>
#include <assert.h>
#include <sys/sysctl.h>

//From: https://developer.apple.com/library/archive/qa/qa1361/_index.html
bool platform_is_debugger_present()
{
    int                 junk;
    int                 mib[4];
    struct kinfo_proc   info;
    size_t              size;

    // Initialize the flags so that, if sysctl fails for some bizarre
    // reason, we get a predictable result.

    info.kp_proc.p_flag = 0;

    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    // Call sysctl.

    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    assert(junk == 0);

    // We're being debugged if the P_TRACED flag is set.

    return ( (info.kp_proc.p_flag & P_TRACED) != 0 );
}

void platform_debug_print(char const* msg) // NOTE(): unused, left here in case I need to remember this API in the future.
{
    @autoreleasepool
    {
        os_log(OS_LOG_DEFAULT, "%{public}s", msg);
    }
}

struct OSX_App_Impl
{
    id helper;
    id delegate;
    __CGEventSource* eventSource;
};

struct OSX_Window_Impl
{
    id object; // id is obj-c void* pointer type, but always points to obj-c obj
    id delegate;
    id view;
    id layer;

    bool maximized = false;
    bool occluded = false;
    bool retina = false;

    bool should_terminate = false;
    bool size_changed_unconsumed = false;

    // Cached window properties to filter out duplicate events
    int width = 0;
    int height = 0;
    int fbWidth = 0;
    int fbHeight = 0;
    float xscale = 0;
    float yscale = 0;

    // The total sum of the distances the cursor has been warped
    // since the last cursor motion event was processed
    // This is kept to counteract Cocoa doing the same internally
    double cursorWarpDeltaX = 0;
    double cursorWarpDeltaY = 0;
};

@interface WindowDelegate : NSObject
{
    OSX_Window_Impl* window;
}

- (instancetype)init:(OSX_Window_Impl*)window;

- (OSX_Window_Impl*)editorWindow;

@end // interface WindowDelegate

@implementation WindowDelegate

- (OSX_Window_Impl*)editorWindow
{
    return window;
}

- (instancetype)init:(OSX_Window_Impl*)new_window
{
    self = [super init];
    if (self != nil)
    {
        self->window = new_window;
    }

    return self;
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    // _glfwInputWindowCloseRequest(window);
    self->window->should_terminate = true;
    return YES;
}

static void update_window_dimensions(OSX_Window_Impl* window)
{
    NSRect bounds = [window->view bounds];
    window->width = bounds.size.width;
    window->height = bounds.size.height;
    window->size_changed_unconsumed = true;

    // [window->object setFrame: frame display: YES animate: NO];

    // NSSize frameSize = CGSizeMake(bounds.size.width, bounds.size.height);
    // [window->view setFrameSize:frameSize];

    // _sapp.framebuffer_width = (int)roundf(bounds.size.width * _sapp.dpi_scale);
    // _sapp.framebuffer_height = (int)roundf(bounds.size.height * _sapp.dpi_scale);
}

- (void)windowDidEndLiveResize:(NSNotification *)notification
{
    update_window_dimensions(self->window);
    // CGRect bounds = CGDisplayBounds(window->monitor->ns.displayID);
    // NSRect frame = NSMakeRect(0,0, window_size.width, window_size.height);
    // [window->object setFrame:frame display:YES];
}

@end // implementation WindowDelegate

@interface ContentView : NSView <NSTextInputClient>
{
    OSX_Window_Impl* window;
    NSTrackingArea* trackingArea;
    NSMutableAttributedString* markedText;
}

- (instancetype)init:(OSX_Window_Impl*)new_window window_rect:(NSRect)window_rect;

@end // interface ContentView

@implementation ContentView

- (instancetype)init:(OSX_Window_Impl *)new_window window_rect:(NSRect)window_rect
{
    self = [super initWithFrame:window_rect];
    if (self != nil)
    {
        self->window = new_window;
        self->trackingArea = nil;
        self->markedText = [[NSMutableAttributedString alloc] init];

        [self updateTrackingAreas];
        [self registerForDraggedTypes:@[NSPasteboardTypeURL]];
    }

    return self;
}

- (BOOL)wantsUpdateLayer {
    return YES;
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
    // NO-OP to statisfy compiler
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
    // NO-OP to statisfy compiler
    return nullptr;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
    // NO-OP to statisfy compiler
    return NSMakeRect(0, 0, 0, 0);
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    // NO-OP to statisfy compiler
    return 0;
}

// - (void)dealloc
// {
//     [trackingArea release];
//     [markedText release];
//     [super dealloc];
// }

- (BOOL)hasMarkedText
{
    return NO;
}

- (NSRange)markedRange
{
    return { NSNotFound, 0 };
}

- (NSRange)selectedRange
{
    return { NSNotFound, 0 };
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange
{
}

- (void)unmarkText
{
}

- (NSArray*)validAttributesForMarkedText
{
    return [NSArray array];
}

@end // implementation ContentView

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end // interface AppDelegate

@implementation AppDelegate

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    // for (_GLFWwindow* window = _glfw.windowListHead;  window;  window = window->next)
        // _glfwInputWindowCloseRequest(window);

    // NSWindow* window = [[sender] mainWindow];
    // TODO(pw): See if we can handle this via the event loop instead.
    NSWindow* window = [[NSApplication sharedApplication] mainWindow];
    if (window != nil)
    {
        WindowDelegate* window_delegate = (__bridge WindowDelegate*)window.delegate;
        OSX_Window_Impl* osx_window = [window_delegate editorWindow];
        osx_window->should_terminate = true;
    }

    return NSTerminateCancel;
}

- (void)applicationDidChangeScreenParameters:(NSNotification *) notification
{
    // for (_GLFWwindow* window = _glfw.windowListHead;  window;  window = window->next)
    // {
    //     if (window->context.client != GLFW_NO_API)
    //         [window->context.nsgl.object update];
    // }

    // _glfwPollMonitorsCocoa();
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
    // if (_glfw.hints.init.ns.menubar)
    // {
    //     // Menu bar setup must go between sharedApplication and finishLaunching
    //     // in order to properly emulate the behavior of NSApplicationMain

    //     if ([[NSBundle mainBundle] pathForResource:@"MainMenu" ofType:@"nib"])
    //     {
    //         [[NSBundle mainBundle] loadNibNamed:@"MainMenu"
    //                                       owner:NSApp
    //                             topLevelObjects:&_glfw.ns.nibObjects];
    //     }
    //     else
    //         createMenuBar();
    // }
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    // _glfwPostEmptyEventCocoa();
    [NSApp stop:nil];
}

- (void)applicationDidHide:(NSNotification *)notification
{
    // for (int i = 0;  i < _glfw.monitorCount;  i++)
        // _glfwRestoreVideoModeCocoa(_glfw.monitors[i]);
}

@end // implementation AppDelegate

@interface AppHelper : NSObject
@end // interface AppHelper

@implementation AppHelper

- (void)selectedKeyboardInputSourceChanged:(NSObject* )object
{
    // updateUnicodeData();
}

- (void)doNothing:(id)object
{
}

@end // AppHelper

Platform_App platform_create_app()
{
    Platform_App app_wrapper = {};
    app_wrapper.impl = new OSX_App_Impl;
    OSX_App_Impl* app = app_wrapper.impl;

    @autoreleasepool
    {
        app->helper = [[AppHelper alloc] init];
        assert(app->helper != nil);

        [NSThread detachNewThreadSelector:@selector(doNothing:)
                        toTarget:app->helper
                        withObject:nil];

        [NSApplication sharedApplication]; // Creates the application instance if it doesnt yet exist.

        app->delegate = [[AppDelegate alloc] init];
        assert(app->delegate != nil);

        [NSApp setDelegate:app->delegate];

        // NSEvent* (^block)(NSEvent*) = ^ NSEvent* (NSEvent* event)
        // {
        //     if ([event modifierFlags] & NSEventModifierFlagCommand)
        //         [[NSApp keyWindow] sendEvent:event];

        //     return event;
        // };

        // _glfw.ns.keyUpMonitor =
        //     [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyUp
        //                                         handler:block];

        // Press and Hold prevents some keys from emitting repeated characters
        // NSDictionary* defaults = @{@"ApplePressAndHoldEnabled":@NO};
        // [[NSUserDefaults standardUserDefaults] registerDefaults:defaults];

        [[NSNotificationCenter defaultCenter] addObserver:app->helper
                                            selector:@selector(selectedKeyboardInputSourceChanged:)
                                            name:NSTextInputContextKeyboardSelectionDidChangeNotification
                                            object:nil];

        // createKeyTables();

        app->eventSource = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
        CGEventSourceSetLocalEventsSuppressionInterval(app->eventSource, 0.0);

        // _glfwPollMonitorsCocoa();

        // if (![[NSRunningApplication currentApplication] isFinishedLaunching])
        // {
        //     [NSApp run];
        // }
    }

    return app_wrapper;
}

void platform_destroy_app(Platform_App app)
{
    delete app.impl;
}

Platform_Window platform_create_window(Platform_App app, Create_Window_Params params)
{
    Platform_Window window_wrapper = {};
    window_wrapper.impl = new OSX_Window_Impl;
    OSX_Window_Impl* window = window_wrapper.impl;

    @autoreleasepool
    {
        window->delegate = [[WindowDelegate alloc] init:window];
        assert(window->delegate != nil);

        NSRect windowRect = NSMakeRect(params.x, params.y, params.width, params.height);

        NSUInteger windowStyle = NSWindowStyleMaskMiniaturizable |
                               NSWindowStyleMaskTitled |
                               NSWindowStyleMaskClosable |
                               NSWindowStyleMaskResizable;
                            //    NSFullSizeContentViewWindowMask;

        window->object = [[NSWindow alloc]
            initWithContentRect:windowRect
            styleMask:windowStyle
            backing:NSBackingStoreBuffered
            defer:NO
        ];
        assert(window->object != nil);

        // NSWindow* ns_window = (__bridge NSWindow*)window->object;
        // ns_window.titlebarAppearsTransparent = YES;

        const NSWindowCollectionBehavior behavior = NSWindowCollectionBehaviorFullScreenPrimary |
                                                    NSWindowCollectionBehaviorManaged;
        [window->object setCollectionBehavior:behavior];

        // [window->object setFrameAutosaveName:@"EditorWindow"];

        [window->object setLevel:NSMainMenuWindowLevel + 1];

        window->view = [[ContentView alloc] init:window window_rect:windowRect];

        NSView* ns_view = (__bridge NSView*)window->view;
        if (![ns_view.layer isKindOfClass:[CAMetalLayer class]])
        {
            CAMetalLayer* layer = [CAMetalLayer layer];
            CGSize viewScale = [ns_view convertSizeToBacking:CGSizeMake(1.0, 1.0)];
            layer.contentsScale = MIN(viewScale.width, viewScale.height);
            [ns_view setLayer:layer];
            [ns_view setWantsLayer:YES];
        }

        // [window->object setFrame:windowRect display:YES];

        window->retina = true;

        [window->object setContentView:window->view];
        [window->object makeFirstResponder:window->view];
        [window->object setTitle:@"Editor"];
        [window->object setDelegate:window->delegate];
        [window->object setAcceptsMouseMovedEvents:YES];
        [window->object setRestorable:NO];

        // NSSize window_size = get_window_size(window);
        // window->width = window_size.width;
        // window->height = window_size.height;

        // _glfwGetWindowSizeCocoa(window, &window->ns.width, &window->ns.height);
        // _glfwGetFramebufferSizeCocoa(window, &window->ns.fbWidth, &window->ns.fbHeight);

        // Show window
        [window->object orderFront:nil];

        // Focus window
        [NSApp activateIgnoringOtherApps:YES];
        [window->object makeKeyAndOrderFront:nil];
    }

    return window_wrapper;
}

void* platform_window_get_raw_handle(Platform_Window window)
{
    NSView* ns_view = (__bridge NSView*)window.impl->view;
    if ([ns_view.layer isKindOfClass:[CAMetalLayer class]])
    {
        return (void*)ns_view.layer;
    }
    else
    {
        LOG("Cant return raw handle for OSX window because it does not have a metal layer.");
        return nullptr;
    }
}

bool platform_did_window_size_change(Platform_Window window)
{
    bool result = window.impl->size_changed_unconsumed;
    window.impl->size_changed_unconsumed = false;
    return result;
}

bool platform_window_closing(Platform_Window window)
{
    return window.impl->should_terminate;
}

void platform_destroy_window(Platform_Window window)
{
    delete window.impl;
}

void platform_pump_events(Platform_App app, Platform_Window main_window)
{
    @autoreleasepool
    {
        for (;;)
        {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                    untilDate:[NSDate distantPast]
                                    inMode:NSDefaultRunLoopMode
                                    dequeue:YES];
            if (event == nil)
            {
                break;
            }

            [NSApp sendEvent:event];
        }
    } // autoreleasepool
}

bool message_box_yes_no(char const* title, char const* message)
{
    @autoreleasepool
    {
        NSAlert* alert = [[NSAlert alloc] init];

        NSString* ns_title = [NSString stringWithCString:title encoding:NSASCIIStringEncoding];
        NSString* ns_msg = [NSString stringWithCString:message encoding:NSASCIIStringEncoding];

        alert.messageText = ns_title;
        alert.informativeText = ns_msg;

        NSButton* yes_button = [alert addButtonWithTitle:@"Yes"];
        NSButton* no_button  = [alert addButtonWithTitle:@"No"];
        UNUSED_VAR(yes_button);
        UNUSED_VAR(no_button);

        NSModalResponse response = [alert runModal];

        return (response == NSAlertFirstButtonReturn);
    }
}

bool platform_get_exe_path(String* path)
{
    ASSERT(path->len > 0);

    u32 buffer_len = path->len;
    s32 retval = _NSGetExecutablePath(path->buffer, &buffer_len);
    if (retval == -1)
    {
        LOG("Failed to get executable path because the out buffer was too small");
    }
    return retval == 0;
}

bool is_file_valid(File_Handle handle)
{
    return handle.handle != nullptr;
}

File_Handle open_file(String path)
{
    if (FILE* handle = fopen(path.buffer, "r"))
    {
        return File_Handle { handle };
    }
    else
    {
        ASSERT_FAILED_MSG("Failed to open file '%s': %s", path.buffer, strerror(errno));
        return File_Handle {};
    }
}

void close_file(File_Handle file)
{
    if (file.handle)
    {
        fclose(file.handle);
    }
}

Option<u64> get_file_size(File_Handle file)
{
    Option<u64> result = {};

    if (file.handle)
    {
        u64 cur_file_pos = ftell(file.handle); // remember where the file head is currently
        if (cur_file_pos != 0)
        {
            fseek(file.handle, 0, SEEK_SET); // set the file headposition back to start incase it isn't currently
        }

        fseek(file.handle, 0, SEEK_END);    // set the position to the end of the file
        u64 file_size = ftell(file.handle); // read how many bytes the position moved from start to end
        option_set(&result, file_size);

        fseek(file.handle, cur_file_pos, SEEK_SET);
    }

    return result;
}

Option<u64> read_file(File_Handle file, Slice<u8> dst, u64 num_bytes)
{
    Option<u64> result = {};
    if (!is_file_valid(file))
    {
        return result;
    }

    u64 bytes_read = fread(dst.array, 1, num_bytes, file.handle);
    if (bytes_read != num_bytes)
    {
        LOG("Did not read expected number of bytes from file: expected=%llu, actual=%llu", num_bytes, bytes_read);
    }

    option_set(&result, bytes_read);
    return result;
}