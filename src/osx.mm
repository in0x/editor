#include "osx.h"

#include "core.h"
#include "memory.h"
#include "platform_shared.h"
#include "stdio.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#import <os/log.h>

#include <assert.h>
#include <mach-o/dyld.h>
#include <pthread.h>
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
    __CGEventSource* eventSource = nullptr;
    Input_State input_state[2] = {};
    pthread_mutex_t input_critsec = {};
    s32 input_idx = 0;
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
    OSX_App_Impl* app;
    NSTrackingArea* trackingArea;
    NSMutableAttributedString* markedText;
}

- (instancetype)init:(OSX_Window_Impl*)new_window window_rect:(NSRect)window_rect;

@end // interface ContentView

@implementation ContentView

- (instancetype)init_window:(OSX_Window_Impl *)new_window init_app:(OSX_App_Impl*)new_app window_rect:(NSRect)window_rect
{
    self = [super initWithFrame:window_rect];
    if (self != nil)
    {
        self->window = new_window;
        self->app = new_app;
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
{}

- (void)unmarkText {}

- (NSArray*)validAttributesForMarkedText
{
    return [NSArray array];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

- (void)mouseDown:(NSEvent *)event 
{
    LOG("Left mouse down");
}

- (void)mouseUp:(NSEvent *)event
{
    LOG("Left mouse up");
}

- (void)mouseDragged:(NSEvent *)event
{
    [self mouseMoved:event]; // forward to mouse moved
}

- (void)mouseMoved:(NSEvent *)event
{
    // Scale the mouse position by the device pixel ratio.
    NSView* ns_view = (__bridge NSView*)window->view;
    CAMetalLayer* layer = [CAMetalLayer layer];
    NSPoint pos = event.locationInWindow;
    s32 x = pos.x * layer.contentsScale;
    s32 y = pos.y * layer.contentsScale;
    y = window->height - y; // OSX origin is bottom left, we put it top left
    // LOG("Mouse moved %d %d", x, y);
}

- (void)rightMouseDown:(NSEvent *)event 
{
    LOG("Right mouse down");
}

- (void)rightMouseUp:(NSEvent *)event
{
    LOG("Right mouse up");
}

- (void)rightMouseDragged:(NSEvent *)event 
{
    [self mouseMoved:event];
}

- (void)otherMouseDown:(NSEvent *)event
{
    LOG("Middle mouse down");
}

- (void)otherMouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)otherMouseUp:(NSEvent *)event
{
    LOG("Middle mouse up");
}

- (void)scrollWheel:(NSEvent *)event
{
    pthread_mutex_lock(&app->input_critsec);
    app->input_state[app->input_idx].scroll_wheel = event.scrollingDeltaY;
    pthread_mutex_unlock(&app->input_critsec);
}

enum OSX_Mod_Mask
{
    OSX_L_SHIFT  = 1 << 1,
    OSX_R_SHIFT  = 1 << 2,
    OSX_L_CTRL   = 1 << 0,
    OSX_R_CTRL   = 1 << 13,
    OSX_L_OPTION = 1 << 5,
    OSX_R_OPTION = 1 << 6,
    OSX_L_CMD    = 1 << 3,
    OSX_R_CMD    = 1 << 4
};

static void handle_mod(Input_State* state, u32 ev_mod_flags, u32 ev_key, u32 ns_mask, 
    u32 l_ns_key, 
    u32 r_ns_key,
    OSX_Mod_Mask l_mod_mask, 
    OSX_Mod_Mask r_mod_mask, 
    Input_Key_Code l_mapped_key, 
    Input_Key_Code r_mapped_key)
{
    if (ns_mask & ev_mod_flags)
    {
        state->key_down[l_mapped_key] = ev_mod_flags & l_mod_mask;
        state->key_down[r_mapped_key] = ev_mod_flags & r_mod_mask;
    }
    else if (ev_key == l_ns_key || ev_key == r_ns_key)
    {
        state->key_down[l_mapped_key] = ev_mod_flags & l_mod_mask;
        state->key_down[r_mapped_key] = ev_mod_flags & r_mod_mask;  
    }
}

void debug_log_modifier_keys(Input_State* input_state)
{
    if (input_state->key_down[Input_Key_Code::L_SHIFT])  LOG("Left shift down");
    if (input_state->key_down[Input_Key_Code::R_SHIFT])  LOG("Right shift down");
    if (input_state->key_down[Input_Key_Code::L_CTRL])   LOG("Left ctrl down");
    if (input_state->key_down[Input_Key_Code::R_CTRL])   LOG("Right ctrl down");
    if (input_state->key_down[Input_Key_Code::L_ALT])    LOG("Left alt down");
    if (input_state->key_down[Input_Key_Code::R_ALT])    LOG("Right alt down");
    if (input_state->key_down[Input_Key_Code::L_CMD])    LOG("Left cmd down");
    if (input_state->key_down[Input_Key_Code::R_CMD])    LOG("Right cmd down");
    if (input_state->key_down[Input_Key_Code::CAPSLOCK]) LOG("capslock down");
}

// Handle modifier keys since they are only registered via modifier flags being set/unset.
- (void) flagsChanged:(NSEvent *) event
{
    pthread_mutex_lock(&app->input_critsec);
    DEFER { pthread_mutex_unlock(&app->input_critsec); };

    if(event.keyCode == 0x39) 
    {
        Input_State* state = &app->input_state[app->input_idx];
        state->key_down[Input_Key_Code::CAPSLOCK] = event.modifierFlags & NSEventModifierFlagCapsLock;
    }

    handle_mod(&app->input_state[app->input_idx], event.modifierFlags, event.keyCode, NSEventModifierFlagShift, 
        0x38, 0x3C, 
        OSX_Mod_Mask::OSX_L_SHIFT, OSX_Mod_Mask::OSX_R_SHIFT, 
        Input_Key_Code::L_SHIFT, Input_Key_Code::R_SHIFT);

    handle_mod(&app->input_state[app->input_idx], event.modifierFlags, event.keyCode, NSEventModifierFlagControl, 
        0x3B, 0x3E, 
        OSX_Mod_Mask::OSX_L_CTRL, OSX_Mod_Mask::OSX_R_CTRL, 
        Input_Key_Code::L_CTRL, Input_Key_Code::R_CTRL);

    handle_mod(&app->input_state[app->input_idx], event.modifierFlags, event.keyCode, NSEventModifierFlagOption, 
        0x3A, 0x3D, 
        OSX_Mod_Mask::OSX_L_OPTION, OSX_Mod_Mask::OSX_R_OPTION, 
        Input_Key_Code::L_ALT, Input_Key_Code::R_ALT);

    handle_mod(&app->input_state[app->input_idx], event.modifierFlags, event.keyCode, NSEventModifierFlagCommand, 
        0x37, 0x36, 
        OSX_Mod_Mask::OSX_L_CMD, OSX_Mod_Mask::OSX_R_CMD, 
        Input_Key_Code::L_CMD, Input_Key_Code::R_CMD);
}

static Input_Key_Code translate_vkey(u32 key_code)
{
    // https://boredzo.org/blog/archives/2007-05-22/virtual-key-codes
    switch (key_code) {
        case 0x00: return Input_Key_Code::A;
        case 0x0B: // letter B
        case 0x08: return Input_Key_Code::Key_Unmapped; // letter C
        case 0x02: return Input_Key_Code::D;
        case 0x0E: // letter E
        case 0x03: // letter F
        case 0x05: // letter G
        case 0x04: // letter H
        case 0x22: // letter I
        case 0x26: // letter J
        case 0x28: // letter K
        case 0x25: // letter L
        case 0x2E: // letter M
        case 0x2D: // letter N
        case 0x1F: // letter O
        case 0x23: // letter P
        case 0x0C: // letter Q
        case 0x0F: return Input_Key_Code::Key_Unmapped; // letter R
        case 0x01: return Input_Key_Code::S;
        case 0x11: // letter T
        case 0x20: // letter U
        case 0x09: return Input_Key_Code::Key_Unmapped; // letter V
        case 0x0D: return Input_Key_Code::W;
        case 0x07: // letter X
        case 0x10: // letter Y
        case 0x06: // letter Z
            return Input_Key_Code::Key_Unmapped;

        case 0x52: // numpad 0
        case 0x53: // numpad 1
        case 0x54: // numpad 2
        case 0x55: // numpad 3
        case 0x56: // numpad 4
        case 0x57: // numpad 5
        case 0x58: // numpad 6
        case 0x59: // numpad 7
        case 0x5B: // numpad 8
        case 0x5C: // numpad 9
            return Input_Key_Code::Key_Unmapped;

        case 0x12: // num key 1
        case 0x13: // num key 2
        case 0x14: // num key 3
        case 0x15: // num key 4
        case 0x17: // num key 5
        case 0x16: // num key 6
        case 0x1A: // num key 7
        case 0x1C: // num key 8
        case 0x19: // num key 9
        case 0x1D: // num key 0
            return Input_Key_Code::Key_Unmapped;

        case 0x7A: // F1
        case 0x78: // F2
        case 0x63: // F3
        case 0x76: // F4
        case 0x60: // F5
        case 0x61: // F6
        case 0x62: // F7
        case 0x64: // F8
        case 0x65: // F9
        case 0x6D: // F10
        case 0x67: // F11
        case 0x6F: // F12
        case 0x6B: // F14
        case 0x71: // F15
        case 0x6A: // F16
        case 0x40: // F17
        case 0x4F: // F18
        case 0x50: // F19
        case 0x5A: // F20
        case 0x27: // apostrophe
        case 0x2A: // backslash
        case 0x2B: // comma;
        case 0x18: // equal / plus
        case 0x32: // grave
        case 0x21: // lbracket 
        case 0x1B: // minus
        case 0x2F: // period
        case 0x1E: // rbracket
        case 0x29: // semicolon
        case 0x2C: // slash
        case 0x33: // backspace
        case 0x39: // capital
        case 0x75: // delete
        case 0x7D: // down
        case 0x77: // end
        case 0x24: return Input_Key_Code::Key_Unmapped; // enter
        case 0x35: return Input_Key_Code::ESC;
        case 0x69: // print
        case 0x73: // home
        case 0x72: // insert
        case 0x7B: // left
        case 0x3A: // lalt
        case 0x3B: // lcontrol
        case 0x38: // lshift
        case 0x37: // lsuper
        case 0x6E: // menu
        case 0x47: // numlock
        case 0x79: // page down
        case 0x74: // page up
        case 0x7C: // right
        case 0x3D: // ralt
        case 0x3E: // rcontrol
        case 0x3C: // rshift
        case 0x36: // rsuper
        case 0x31: // space
        case 0x30: // tab
        case 0x7E: // up
        case 0x45: // add
        case 0x41: // decimal
        case 0x4B: // divide
        case 0x4C: // enter
        case 0x51: // numpad_equal
        case 0x43: // multiply
        case 0x4E: // subtract
            return Input_Key_Code::Key_Unmapped;
        default: { ASSERT_FAILED_MSG("Unknown virtual key"); return Input_Key_Code::Key_Unmapped; }
    }
}

- (void)keyDown:(NSEvent *)event
{
    Input_Key_Code key = translate_vkey(event.keyCode);

    pthread_mutex_lock(&app->input_critsec);
    app->input_state[app->input_idx].key_down[key] = true;
    pthread_mutex_unlock(&app->input_critsec);
}

- (void)keyUp:(NSEvent *)event
{
    Input_Key_Code key = translate_vkey(event.keyCode);

    pthread_mutex_lock(&app->input_critsec);
    app->input_state[app->input_idx].key_down[key] = false;
    pthread_mutex_unlock(&app->input_critsec);
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

    pthread_mutex_init(&app->input_critsec, nullptr);
    
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
    pthread_mutex_destroy(&app.impl->input_critsec);
    delete app.impl;
}

Screen_Props platform_get_main_window_props()
{
    NSScreen* mainScreen = [NSScreen mainScreen];
    NSRect screenFrame = [mainScreen frame];

    Screen_Props props = {};
    props.width = screenFrame.size.width;
    props.height = screenFrame.size.height;    
    return props;
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

        Screen_Props main_screen_props = platform_get_main_window_props();
        s32 win_orig_y = main_screen_props.height - params.y - params.height;
        s32 win_orig_x = params.x;
        NSRect windowRect = NSMakeRect(win_orig_x, win_orig_y, params.width, params.height);

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

        window->view = [[ContentView alloc] init_window:window init_app:app.impl window_rect:windowRect];

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

        window->width = params.width;
        window->height = params.height;
        window->retina = true;
        [window->object setContentView:window->view];
        [window->object makeFirstResponder:window->view];
        [window->object setDelegate:window->delegate];
        [window->object setAcceptsMouseMovedEvents:YES];
        [window->object setRestorable:NO];

        if (params.title)
        {
            NSString* ns_title = [NSString stringWithCString:params.title encoding:NSASCIIStringEncoding];
            [window->object setTitle:ns_title];
        }
        else
        {
            [window->object setTitle:@"Untitled"];
        }

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

Input_State const* platform_pump_events(Platform_App app, Platform_Window main_window)
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

    pthread_mutex_lock(&app.impl->input_critsec);
    s32 read_idx = app.impl->input_idx;
    app.impl->input_idx = (app.impl->input_idx + 1) % 2;
    memcpy( // ensure the buffer we're about to read is synced with the most recent writes, so input events stay in sync.
        &app.impl->input_state[app.impl->input_idx],
        &app.impl->input_state[read_idx], 
        sizeof(Input_State));
    pthread_mutex_unlock(&app.impl->input_critsec);
    return &app.impl->input_state[read_idx];
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

Option<u64> read_file(File_Handle file, Array<u8> dst, u64 num_bytes)
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