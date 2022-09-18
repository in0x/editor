#include "core.h"
#include "platform.h"
#include "vk.h"
#include "shader_compiler.h"

// TODO(phil):
// 1) move the rest of the windows code from here into win32.h
// 2) swap out win32 window for osx window
// 3) get the code compiling under clang
// 4) get the code linking with vulkan
// 5) get the window handle from the platform window and feed it to vulkan
// 6) get the triangle rendering again


static VkBool32 VKAPI_CALL debug_report_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, 
	u64 object, size_t location, s32 message_code, char const* layer_prefix, char const* message, void* user_data)
{
	bool is_error = flags & VK_DEBUG_REPORT_ERROR_BIT_EXT;

	LOG("[VK] SEV: %s LAYER: %s MSG: %s", is_error ? "ERROR" : "WARNING", layer_prefix, message);

	ASSERT_MSG(!is_error, "Vulkan Validation Error found!");

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

#if PLATFORM_WIN32
INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) 
#else
int main(int argc, char** argv)
{
    Platform_App platform_app = platform_create_app();

	String root_dir = alloc_string(MAX_PATH);
	DEFER { free_string(root_dir); };
	{
        bool success = platform_get_exe_path(&root_dir);
        ASSERT_MSG(success, "Failed to get path to exe, got %s", root_dir.buffer ? root_dir.buffer : "{null}");
        
		char* exe_path = strstr(root_dir.buffer, "editor");
		exe_path += strlen("editor\\");
		*exe_path = '\0';

		LOG("Root directory: \"%s\"", root_dir);
	}

	auto make_abs_path = [&root_dir](char const* rel_path, char* dst, u32 dst_size)
	{
		// TODO(): Itd be nice to make this into a path combine that guarantees delimiters
		snprintf(dst, dst_size, "%s%s", root_dir.buffer, rel_path);
    };

	VK_CHECK(volkInitialize());

    // TODO(platform_port): Create window throught platform agnostic path
    // and extract window handle for vulkan
    
    // Create_Window_Params window_params = {};
    // window_params.x = 50;
    // window_params.y = 50;
    // window_params.width  = 800;
    // window_params.height = 800;
    // window_params.class_name = L"editor_window_class";
    // window_params.title = L"Editor";

    // HWND main_window_handle = create_window(&window_params);
        
    // if (main_window_handle == INVALID_HANDLE_VALUE)
    // {
    //     return -1;
    // }

    // ShowWindow((HWND)main_window_handle, SW_SHOW);
    // SetForegroundWindow((HWND)main_window_handle);
    // UpdateWindow((HWND)main_window_handle);

    Platform_Window main_window_handle = platform_create_window(platform_app);
    
    VkInstance vk_instance = VK_NULL_HANDLE;
	{
        VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
        app_info.apiVersion = C_TARGET_VK_VERSION;

        VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        create_info.pApplicationInfo = &app_info;

#if DEBUG_BUILD
        char const* debug_layers[] = { "VK_LAYER_KHRONOS_validation" };
        create_info.ppEnabledLayerNames = debug_layers;
        create_info.enabledLayerCount = ARRAYSIZE(debug_layers);
#endif

        char const* extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
		    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#if PLATFORM_WIN32
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif PLATFORM_OSX
            VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
			VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
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

#if PLATFORM_WIN32
			if (!vkGetPhysicalDeviceWin32PresentationSupportKHR(phys_devices[i], family_idx))
			{
				continue;
			}
#endif
            // According to spec: "On macOS, all physical devices and queue families must be capable of
            // presentation with any layer. As a result there is no macOS-specific query for these
            // capabilities."
            
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

	volkLoadDevice(vk_device);

	VkSurfaceKHR vk_surface = VK_NULL_HANDLE;

#if PLATFORM_WIN32

    VkWin32SurfaceCreateInfoKHR create_info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    create_info.hinstance = GetModuleHandle(nullptr);
    create_info.hwnd = main_window_handle;

    VK_CHECK(vkCreateWin32SurfaceKHR(vk_instance, &create_info, nullptr, &vk_surface));

#elif PLATFORM_OSX

    VkMacOSSurfaceCreateInfoMVK create_info = { VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK };
    create_info.pView = platform_window_get_raw_handle(main_window_handle);

    VK_CHECK(vkCreateMacOSSurfaceMVK(vk_instance, &create_info, nullptr, &vk_surface));
#endif

    VK_ASSERT_VALID(vk_surface);

	VkBool32 present_supported = false;
	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vk_phys_device, family_idx, vk_surface, &present_supported));
	ASSERT(present_supported);

	VkFormat swapchain_fmt = VK_FORMAT_UNDEFINED;
	{
		u32 fmt_count = 0;
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_phys_device, vk_surface, &fmt_count, nullptr));

		VkSurfaceFormatKHR* fmts = new VkSurfaceFormatKHR[fmt_count];
		DEFER{ delete[] fmts; };

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
		VkAttachmentDescription attachment = {};
		attachment.format = swapchain_fmt;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment.finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference color_attachment = {};
		color_attachment.attachment = 0;
		color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment;

		VkRenderPassCreateInfo create_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		create_info.attachmentCount = 1;
		create_info.pAttachments = &attachment;
		create_info.subpassCount = 1;
		create_info.pSubpasses = &subpass;

		VK_CHECK(vkCreateRenderPass(vk_device, &create_info, nullptr, &vk_render_pass));
	}
	VK_ASSERT_VALID(vk_render_pass);

	shader_compiler_init();

    char shader_path[MAX_PATH] = "\0";
    
    make_abs_path("src\\shaders\\triangle.vert.glsl", shader_path, MAX_PATH);
	VkShaderModule vert_shader = compile_shader(vk_device, Shader_Stage::vertex, shader_path);

    make_abs_path("src\\shaders\\triangle.frag.glsl", shader_path, MAX_PATH);
	VkShaderModule frag_vshader = compile_shader(vk_device, Shader_Stage::fragment, shader_path);
	
    while (!platform_window_closing(main_window_handle))
    {
        platform_pump_events(platform_app, main_window_handle);
    }
    
    // close_window(main_window_handle, &window_params);
    platform_destroy_window(main_window_handle);
    
	shader_compiler_shutdown();

    vkDestroyDebugReportCallbackEXT(vk_instance, vk_dbg_callback, nullptr);
    vkDestroyInstance(vk_instance, nullptr);

    platform_destroy_app(platform_app);
    
    return 0;
}
#endif