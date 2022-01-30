#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h> // TODO: hide in platform impl file

#include "volk/volk.h"

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

#ifdef _DEBUG
    #define DEBUG_BUILD 1
#else
    #define DEBUG_BUILD 0
#endif

#define CONCAT_INNER(l, r) l ## r
#define CONCAT(l, r) CONCAT_INNER(l, r)

#define UNIQUE_ID(x) CONCAT(x, __COUNTER__)

#define STRINGIFY_INNER(x) #x
#define STRINGIFY(x) STRINGIFY_INNER(x)

template <typename T>
struct DeferredFunction
{
	DeferredFunction(T&& closure)
		: closure(static_cast<T&&>(closure))
	{}

	~DeferredFunction()
	{
		closure();
	}

	T closure;
};

#define DEFER DeferredFunction const UNIQUE_ID(_scope_exit) = [&] 

enum Print_Flags
{
	NONE = 0,
	APPEND_NEWLINE = 0x1,
};

// NOTE(): Returned pointer must be deleted by caller.
char const* inplace_printf(char const* fmt, Print_Flags flags, va_list args)
{
	bool append_newline = flags & Print_Flags::APPEND_NEWLINE;

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
	char const* user_msg = nullptr;
	DEFER { delete user_msg; };
	if (msg)
	{
		va_list user_args;
		va_start(user_args, msg);
		user_msg = inplace_printf(msg, Print_Flags::NONE, user_args);
		va_end(user_args);
	}
	
	char const* assert_msg = nullptr;
	DEFER { delete assert_msg; };
	if (user_msg)
	{
		assert_msg = va_inplace_printf("Condition: %s\nMessage: %s", Print_Flags::APPEND_NEWLINE, condition, user_msg);
	}
	else
	{
		assert_msg = va_inplace_printf("Condition: %s", Print_Flags::APPEND_NEWLINE, condition);
	}

	bool should_break = (IDYES == MessageBoxA(NULL, assert_msg, "Assert Failed! Break into code?", MB_YESNO | MB_ICONERROR));

	return should_break;
}

#define ASSERT_MSG(condition, msg, ...) if ((condition) == false)  \
			if (handle_assert( #condition , msg, __VA_ARGS__)) \
				__debugbreak();								   \

#define ASSERT(condition) if ((condition) == false)   \
			if (handle_assert( #condition, nullptr )) \
				__debugbreak();						  \

void log_message(char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	// TODO: we dont want to allocate on every log, so reserve a logging buffer instead.
	// will need to update inplace_printf to handle truncation (if not write to end of write, cant assume full buffer is used)
	char const* msg = inplace_printf(fmt, Print_Flags::APPEND_NEWLINE, args);
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
        log_last_windows_error();
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
        log_last_windows_error();
        ASSERT_MSG(window_handle != INVALID_HANDLE_VALUE, "Failed to create main window!");
    }

    return (HWND)window_handle;
}

static void close_window(HWND handle, Create_Window_Params* params)
{
    UnregisterClass(params->class_name, GetModuleHandle(nullptr));
}

#define VK_CHECK(op) \
	do { \
		VkResult result = op; \
		ASSERT_MSG(result == VK_SUCCESS, "Error code: %d", result); \
	} while (false)

#define VK_ASSERT_VALID(handle) ASSERT(handle != VK_NULL_HANDLE)

static VkBool32 VKAPI_CALL debug_report_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, 
	u64 object, size_t location, s32 message_code, char const* layer_prefix, char const* message, void* user_data)
{
	bool is_error = flags & VK_DEBUG_REPORT_ERROR_BIT_EXT;

	LOG("[VK] SEV: %s LAYER: %s MSG: %s", is_error ? "ERROR" : "WARNING", layer_prefix, message);

	ASSERT(!is_error);

	return VK_FALSE; // Spec states users should always return false here.
}

u32 get_gfx_family_index(VkPhysicalDevice phys_device)
{
	u32 queue_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_count, nullptr);

	VkQueueFamilyProperties* queue_props = new VkQueueFamilyProperties[queue_count];
	DEFER{ delete[] queue_props; };
	vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_count, queue_props);

	for (u32 i = 0; i < queue_count; ++i)
	{
		if (queue_props[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
		{
			return i;
		}
	}

	return VK_QUEUE_FAMILY_IGNORED;
}

INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) 
{
	VK_CHECK(volkInitialize());

    Create_Window_Params window_params = {};
    window_params.x = 50;
    window_params.y = 50;
    window_params.width  = 800;
    window_params.height = 800;
    window_params.class_name = L"editor_window_class";
    window_params.title = L"Editor";

    HWND main_window_handle = create_window(&window_params);
        
    if (main_window_handle == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    ShowWindow((HWND)main_window_handle, SW_SHOW);
    SetForegroundWindow((HWND)main_window_handle);
    UpdateWindow((HWND)main_window_handle);

    VkInstance vk_instance = VK_NULL_HANDLE;
	{
        VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
        app_info.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        create_info.pApplicationInfo = &app_info;

#if DEBUG_BUILD
        char const* debug_layers[] = { "VK_LAYER_KHRONOS_validation" };
        create_info.ppEnabledLayerNames = debug_layers;
        create_info.enabledLayerCount = ARRAYSIZE(debug_layers);
#endif

        char const* extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
		    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        };

        create_info.ppEnabledExtensionNames = extensions;
        create_info.enabledExtensionCount = ARRAYSIZE(extensions);

        VK_CHECK(vkCreateInstance(&create_info, nullptr, &vk_instance));
	}
    ASSERT(vk_instance != VK_NULL_HANDLE);
	
    volkLoadInstanceOnly(vk_instance);

#if DEBUG_BUILD
	VkDebugReportCallbackEXT vk_dbg_callback = VK_NULL_HANDLE;
	{
		VkDebugReportCallbackCreateInfoEXT create_info = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT };
		create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		create_info.pfnCallback = debug_report_callback;

		VK_CHECK(vkCreateDebugReportCallbackEXT(vk_instance, &create_info, nullptr, &vk_dbg_callback));
	}
    ASSERT(vk_dbg_callback != VK_NULL_HANDLE);
#endif // DEBUG_BUILD

	// Create vkDevice

	VkPhysicalDevice vk_phys_device = VK_NULL_HANDLE;
	{
		VkPhysicalDevice phys_devices[8];
		u32 phys_device_count = ARRAYSIZE(phys_devices);
		VK_CHECK(vkEnumeratePhysicalDevices(vk_instance, &phys_device_count, phys_devices));

		VkPhysicalDevice discrete_gpu = VK_NULL_HANDLE;
		VkPhysicalDevice fallback_gpu = VK_NULL_HANDLE;

		for (u32 i = 0; i < phys_device_count; ++i)
		{
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(phys_devices[i], &props);

			LOG("Enumerating GPU %s", props.deviceName);
			
			u32 family_idx = get_gfx_family_index(phys_devices[i]);
			if (family_idx == VK_QUEUE_FAMILY_IGNORED)
			{
				continue;
			}

			if (!vkGetPhysicalDeviceWin32PresentationSupportKHR(phys_devices[i], family_idx))
			{
				continue;
			}

			if (!discrete_gpu)
			{
				if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				{
					discrete_gpu = phys_devices[i];
					LOG("Found discrete GPU %s", props.deviceName);
				}
			}

			if (!fallback_gpu)
			{
				fallback_gpu = phys_devices[i];
				LOG("Found fallback GPU %s", props.deviceName);
			}
		}

		vk_phys_device = discrete_gpu ? discrete_gpu : fallback_gpu;
	}
	ASSERT_MSG(vk_phys_device != VK_NULL_HANDLE, "No valid GPU device found!");
	
	u32 family_idx = get_gfx_family_index(vk_phys_device);
	ASSERT(family_idx != VK_QUEUE_FAMILY_IGNORED);

	VkDevice vk_device = VK_NULL_HANDLE;
	{
		f32 queue_prios[] = { 1.0f };

		VkDeviceQueueCreateInfo queue_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		queue_info.queueFamilyIndex = family_idx;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = queue_prios;

		char const* extensions[] =
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
		};

		VkPhysicalDeviceFeatures features = {};
		features.vertexPipelineStoresAndAtomics = true;

		VkDeviceCreateInfo create_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		create_info.queueCreateInfoCount = 1;
		create_info.pQueueCreateInfos = &queue_info;
		create_info.ppEnabledExtensionNames = extensions;
		create_info.enabledExtensionCount = ARRAYSIZE(extensions);
		create_info.pEnabledFeatures = &features;

		VK_CHECK(vkCreateDevice(vk_phys_device, &create_info, nullptr, &vk_device));
	}
	VK_ASSERT_VALID(vk_device);

	VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
	{
		VkWin32SurfaceCreateInfoKHR create_info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
		create_info.hinstance = GetModuleHandle(nullptr);
		create_info.hwnd = main_window_handle;

		VK_CHECK(vkCreateWin32SurfaceKHR(vk_instance, &create_info, nullptr, &vk_surface));
	}
	VK_ASSERT_VALID(vk_surface);

	VkBool32 present_supported = false;
	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vk_phys_device, family_idx, vk_surface, &present_supported));
	ASSERT(present_supported);

	VkFormat swapchain_fmt = VK_FORMAT_UNDEFINED;
	{
		u32 fmt_count = 0;
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_phys_device, vk_surface, &fmt_count, nullptr));

		VkSurfaceFormatKHR* fmts = new VkSurfaceFormatKHR[fmt_count];
		DEFER{ delete fmts; };

		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_phys_device, vk_surface, &fmt_count, fmts));
		
		if ((fmt_count == 1) && (fmts[0].format == VK_FORMAT_UNDEFINED))
		{
			swapchain_fmt =  VK_FORMAT_R8G8B8A8_UNORM;
		}
		else
		{
			for (u32 i = 0; i < fmt_count; ++i)
			{
				if (fmts[i].format == VK_FORMAT_R8G8B8A8_UNORM || 
					fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM)
				{
					swapchain_fmt = fmts[i].format;
				}
			}
		}

		if (swapchain_fmt == VK_FORMAT_UNDEFINED)
		{
			swapchain_fmt = fmts[0].format;
		}
	}

	VkSemaphore acq_semaphore = VK_NULL_HANDLE;
	VkSemaphore rel_semaphore = VK_NULL_HANDLE;
	{
		VkSemaphoreCreateInfo create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VK_CHECK(vkCreateSemaphore(vk_device, &create_info, nullptr, &acq_semaphore));
		VK_CHECK(vkCreateSemaphore(vk_device, &create_info, nullptr, &rel_semaphore));
		VK_ASSERT_VALID(acq_semaphore);
		VK_ASSERT_VALID(rel_semaphore);
	}

	VkQueue vk_queue = VK_NULL_HANDLE;
	vkGetDeviceQueue(vk_device, family_idx, 0, &vk_queue);

	VkRenderPass vk_render_pass = VK_NULL_HANDLE;
	{
	}
		
	/*volkLoadDevice(vk_device);*/ // Do we need this?

	bool exit_app = false;    
    MSG msg = {};
	while (!exit_app)
	{
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (WM_QUIT == msg.message)
			{
				exit_app = true;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

    close_window(main_window_handle, &window_params);

    vkDestroyDebugReportCallbackEXT(vk_instance, vk_dbg_callback, nullptr);
    vkDestroyInstance(vk_instance, nullptr);
	return 0;
}