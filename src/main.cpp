#include "core.h"
#include "mathlib.h"
#include "memory.h"
#include "platform.h"
#include "shader_compiler.h"
#include "vk.h"

#define ASSERT_IF_ERROR_ELSE_LOG(condition, fmt_string, ...) \
    if (condition)                                           \
    {                                                        \
        ASSERT_FAILED_MSG(fmt_string, __VA_ARGS__);          \
    }                                                        \
    else                                                     \
    {                                                        \
        LOG(fmt_string, __VA_ARGS__);                        \
    }

static VkBool32 VKAPI_CALL debug_report_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type,
                                                 u64 object, size_t location, s32 message_code, char const* layer_prefix, char const* message, void* user_data)
{
    bool is_error = flags & VK_DEBUG_REPORT_ERROR_BIT_EXT;
    ASSERT_IF_ERROR_ELSE_LOG(
        is_error,
        "[VK] SEV: %s LAYER: %s | MSG: %s",
        is_error ? "ERROR" : "WARNING", layer_prefix, message);

    return VK_FALSE; // Spec states users should always return false here.
}

static VkBool32 VKAPI_CALL debug_message_callback(VkDebugUtilsMessageSeverityFlagBitsEXT msg_sev, VkDebugUtilsMessageTypeFlagsEXT msg_type,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    char const* msg_type_name = nullptr;
    switch (msg_type)
    {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: // Some event has happened that is unrelated to the specification or performance
        msg_type_name = "General";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: // Something has happened that violates the specification or indicates a possible mistake
        msg_type_name = "Validation";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: // Potential non-optimal use of Vulkan
        msg_type_name = "Perf";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_FLAG_BITS_MAX_ENUM_EXT: // MoltenVK sends messages of this kind
        msg_type_name = "MoltenVK";
        break;
    default:
    {
        msg_type_name = "Unkown Message Type";
        ASSERT_FAILED_MSG("Unhandled vulkan debug message type");
        break;
    }
    }

    char const* msg_sev_name = nullptr;
    switch (msg_sev)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        msg_sev_name = "Verbose";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        msg_sev_name = "Info";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        msg_sev_name = "Warning";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        msg_sev_name = "Error";
        break;
    default:
    {
        msg_sev_name = "Unknown Verbosity";
        ASSERT_FAILED_MSG("Unhandled vulkan verbosity type");
        break;
    }
    }

    ASSERT_IF_ERROR_ELSE_LOG(
        msg_sev == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        "[VK %s] Sev: %s | Msg: %s",
        msg_type_name,
        msg_sev_name,
        callback_data->pMessage);

    return VK_FALSE; // Users should always return false according to spec.
}

u32 get_gfx_family_index(VkPhysicalDevice phys_device, Arena* arena)
{
    ARENA_DEFER_CLEAR(arena);

    u32 queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_count, nullptr);

    Slice<VkQueueFamilyProperties> queue_props = arena_push_array<VkQueueFamilyProperties>(arena, queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_count, queue_props.array);

    for (u32 i = 0; i < queue_count; ++i)
    {
        if (queue_props[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
        {
            return i;
        }
    }

    return VK_QUEUE_FAMILY_IGNORED;
}

VkImageMemoryBarrier create_image_barrier(
    VkImage image,
    VkAccessFlags src_access_mask,
    VkAccessFlags dst_access_mask,
    VkImageLayout old_layout,
    VkImageLayout new_layout)
{
    VkImageMemoryBarrier result = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};

    result.srcAccessMask = src_access_mask;
    result.dstAccessMask = dst_access_mask;
    result.oldLayout = old_layout;
    result.newLayout = new_layout;
    result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.image = image;
    result.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    result.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return result;
}

static VkInstance create_vk_instance()
{
    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.apiVersion = C_TARGET_VK_VERSION;

    VkInstanceCreateInfo create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pApplicationInfo = &app_info;

#if DEBUG_BUILD
    char const* debug_layers[] = {"VK_LAYER_KHRONOS_validation"};
    create_info.ppEnabledLayerNames = debug_layers;
    create_info.enabledLayerCount = ARRAYSIZE(debug_layers);
#endif

    char const* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#if PLATFORM_WIN32
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif PLATFORM_OSX
        "VK_EXT_metal_surface",
        VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#endif
    };

    create_info.ppEnabledExtensionNames = extensions;
    create_info.enabledExtensionCount = ARRAYSIZE(extensions);

    VkInstance vk_instance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&create_info, nullptr, &vk_instance));
    return vk_instance;
}

static bool are_strings_same_nocase(char const* lhs, char const* rhs)
{
#if PLATFORM_WIN32
    return _stricmp(lhs, rhs) == 0;
#else
    return strcasecmp(lhs, rhs) == 0;
#endif
}

static VkPhysicalDevice create_vk_physical_device(VkInstance vk_instance, VkSurfaceKHR vk_surface, Slice<char const*> desired_extensions, Arena* arena)
{
    u32 phys_device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(vk_instance, &phys_device_count, nullptr));

    ARENA_DEFER_CLEAR(arena);
    Slice<VkPhysicalDevice> phys_devices = arena_push_array<VkPhysicalDevice>(arena, phys_device_count);

    VK_CHECK(vkEnumeratePhysicalDevices(vk_instance, &phys_device_count, phys_devices.array));

    VkPhysicalDevice discrete_gpu = VK_NULL_HANDLE;
    VkPhysicalDevice fallback_gpu = VK_NULL_HANDLE;

    for (u32 i = 0; i < phys_device_count; ++i)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(phys_devices[i], &props);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(phys_devices[i], &features);

        LOG("Enumerating GPU %s", props.deviceName);

        u32 gfx_family_idx = get_gfx_family_index(phys_devices[i], arena);
        if (gfx_family_idx == VK_QUEUE_FAMILY_IGNORED)
        {
            continue;
        }

#if PLATFORM_WIN32
        if (!vkGetPhysicalDeviceWin32PresentationSupportKHR(phys_devices[i], gfx_family_idx))
        {
            continue;
        }
#else
        VkBool32 present_supported = false;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(phys_devices[i], gfx_family_idx, vk_surface, &present_supported));
        if (!present_supported)
        {
            continue;
        }
#endif
        // According to spec: "On macOS, all physical devices and queue families must be capable of
        // presentation with any layer. As a result there is no macOS-specific query for these
        // capabilities."

        u32 ext_count = 0;
        vkEnumerateDeviceExtensionProperties(phys_devices[i], nullptr, &ext_count, nullptr);

        Slice<VkExtensionProperties> available_exts = arena_push_array<VkExtensionProperties>(arena, ext_count);
        vkEnumerateDeviceExtensionProperties(phys_devices[i], nullptr, &ext_count, available_exts.array);

        bool has_all_exts = true;
        for (char const* ext_to_find : desired_extensions)
        {
            bool found_ext = false;
            for (VkExtensionProperties const& ext_props : available_exts)
            {
                if (are_strings_same_nocase(ext_to_find, ext_props.extensionName))
                {
                    found_ext = true;
                    break;
                }
            }

            if (!found_ext)
            {
                has_all_exts = false;
                break;
            }
        }

        if (!has_all_exts)
            continue;

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

    VkPhysicalDevice vk_phys_device = VK_NULL_HANDLE;
    vk_phys_device = discrete_gpu ? discrete_gpu : fallback_gpu;
    ASSERT_MSG(vk_phys_device != VK_NULL_HANDLE, "No valid GPU device found!");

    return vk_phys_device;
}

static VkDevice create_vk_device(VkInstance vk_instance, VkPhysicalDevice vk_phys_device, u32 queue_family_idx)
{
    f32 queue_prios[] = {1.0f};

    VkDeviceQueueCreateInfo queue_info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = queue_family_idx;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = queue_prios;

    char const* extensions[] =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
#if PLATFORM_OSX
        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
    };

    VkPhysicalDeviceFeatures features = {};
    features.vertexPipelineStoresAndAtomics = true;

    VkDeviceCreateInfo create_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_info;
    create_info.ppEnabledExtensionNames = extensions;
    create_info.enabledExtensionCount = ARRAYSIZE(extensions);
    create_info.pEnabledFeatures = &features;

    VkDevice vk_device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(vk_phys_device, &create_info, nullptr, &vk_device));
    return vk_device;
}

struct Vulkan_Debug_Utils
{
    VkDebugReportCallbackEXT report_callback = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
};

static Vulkan_Debug_Utils vk_create_debug_utils(VkInstance vk_instance)
{
    Vulkan_Debug_Utils vk_debug_utils = {};

    VkDebugReportCallbackCreateInfoEXT report_create_info = {VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT};
    report_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    report_create_info.pfnCallback = debug_report_callback;

    VK_CHECK(vkCreateDebugReportCallbackEXT(vk_instance, &report_create_info, nullptr, &vk_debug_utils.report_callback));

    VkDebugUtilsMessengerCreateInfoEXT msger_create_info = {};
    msger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    msger_create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    msger_create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    msger_create_info.pfnUserCallback = debug_message_callback;

    VK_CHECK(vkCreateDebugUtilsMessengerEXT(vk_instance, &msger_create_info, nullptr, &vk_debug_utils.messenger));

    ASSERT(vk_debug_utils.report_callback != VK_NULL_HANDLE);
    ASSERT(vk_debug_utils.messenger != VK_NULL_HANDLE);
    return vk_debug_utils;
}

static VkSurfaceKHR create_vk_surface(VkInstance vk_instance, void* main_window_handle)
{
    VkSurfaceKHR vk_surface = VK_NULL_HANDLE;

#if PLATFORM_WIN32
    VkWin32SurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    create_info.hinstance = GetModuleHandle(nullptr);
    create_info.hwnd = main_window_handle;
    VK_CHECK(vkCreateWin32SurfaceKHR(vk_instance, &create_info, nullptr, &vk_surface));
#elif PLATFORM_OSX
    VkMacOSSurfaceCreateInfoMVK create_info = {VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK};
    create_info.pView = main_window_handle;
    VK_CHECK(vkCreateMacOSSurfaceMVK(vk_instance, &create_info, nullptr, &vk_surface));
#endif

    VK_ASSERT_VALID(vk_surface);
    return vk_surface;
}

static VkFormat get_swapchain_fmt(VkPhysicalDevice vk_phys_device, VkSurfaceKHR vk_surface, Arena* arena)
{
    ARENA_DEFER_CLEAR(arena);

    VkFormat swapchain_fmt = VK_FORMAT_UNDEFINED;

    u32 fmt_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_phys_device, vk_surface, &fmt_count, nullptr));

    Slice<VkSurfaceFormatKHR> fmts = arena_push_array<VkSurfaceFormatKHR>(arena, fmt_count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_phys_device, vk_surface, &fmt_count, fmts.array));

    if ((fmt_count == 1) && (fmts[0].format == VK_FORMAT_UNDEFINED))
    {
        swapchain_fmt = VK_FORMAT_R8G8B8A8_UNORM;
    }
    else
    {
        for (VkSurfaceFormatKHR const& test_fmt : fmts)
        {
            bool has_rgba8 = test_fmt.format == VK_FORMAT_R8G8B8A8_UNORM || test_fmt.format == VK_FORMAT_B8G8R8A8_UNORM;
            bool has_srgb = test_fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            if (has_rgba8 && has_srgb)
            {
                swapchain_fmt = test_fmt.format;
            }
        }
    }

    if (swapchain_fmt == VK_FORMAT_UNDEFINED)
    {
        swapchain_fmt = fmts[0].format;
    }

    return swapchain_fmt;
}

static VkSwapchainKHR create_vk_swapchain(VkDevice vk_device, VkSurfaceKHR vk_surface, VkFormat swapchain_fmt, u32 gfx_family_idx, u32 width, u32 height)
{
    VkSwapchainCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create_info.surface = vk_surface;
    create_info.minImageCount = 2;
    create_info.imageFormat = swapchain_fmt;
    create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create_info.imageExtent.width = width;
    create_info.imageExtent.height = height;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //  VK_IMAGE_USAGE_TRANSFER_DST_BIT when we want to post-process first and use a compute copy
    create_info.queueFamilyIndexCount = 1;
    create_info.pQueueFamilyIndices = &gfx_family_idx;
    create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;

    VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(vk_device, &create_info, nullptr, &vk_swapchain));

    return vk_swapchain;
}

static VkRenderPass create_vk_fullframe_renderpass(VkDevice vk_device, VkFormat swapchain_fmt)
{
    VkAttachmentDescription attachment = {};
    attachment.format = swapchain_fmt;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment = {};
    color_attachment.attachment = 0;
    color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment;

    VkRenderPassCreateInfo create_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    create_info.attachmentCount = 1;
    create_info.pAttachments = &attachment;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;

    VkRenderPass vk_render_pass = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(vk_device, &create_info, nullptr, &vk_render_pass));
    VK_ASSERT_VALID(vk_render_pass);
    return vk_render_pass;
}

void print_row(Matrix4 const& m, u32 row)
{
    LOG("% 03.3f % 03.3f % 03.3f % 03.3f", m(row,0), m(row,1), m(row,2), m(row,3));
}

void print_matrix(Matrix4 const& m)
{
    print_row(m, 0);
    print_row(m, 1);
    print_row(m, 2);
    print_row(m, 3);
    LOG("");
}

#if PLATFORM_WIN32
INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
#else
int main(int argc, char** argv)
{
    Arena program_arena = arena_allocate(1024 * 1024); // Give ourselves one MiB of main memory.
    DEFER { arena_free(&program_arena); };

    Platform_App platform_app = platform_create_app();

    char root_dir[MAX_PATH];
    {
        String dir_string{root_dir, MAX_PATH};
        bool success = platform_get_exe_path(&dir_string);
        ASSERT_MSG(success, "Failed to get path to exe, got %s", root_dir);

        char* exe_path = strstr(root_dir, "editor");
        exe_path += strlen("editor/");
        *exe_path = '\0';

        LOG("Root directory: \"%s\"", root_dir);
    }

    test_matrix4_mul();

    VK_CHECK(volkInitialize());

    VkInstance vk_instance = create_vk_instance();
    volkLoadInstanceOnly(vk_instance);

    Create_Window_Params window_params = {};
    window_params.x = 200;
    window_params.y = 400;
    window_params.width = 500;
    window_params.height = 500;
    // window_params.class_name = L"editor_window_class";
    window_params.title = "Title of the Window";
    Platform_Window main_window_handle = platform_create_window(platform_app, window_params);

#if DEBUG_BUILD
    Vulkan_Debug_Utils vk_debug_utils = vk_create_debug_utils(vk_instance);
#endif // DEBUG_BUILD

    VkSurfaceKHR vk_surface = create_vk_surface(vk_instance, platform_window_get_raw_handle(main_window_handle));

    char const* phys_device_exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    Slice<char const*> phys_device_ext_slice{&phys_device_exts, 1};
    VkPhysicalDevice vk_phys_device = create_vk_physical_device(vk_instance, vk_surface, phys_device_ext_slice, &program_arena);

    u32 const gfx_family_idx = get_gfx_family_index(vk_phys_device, &program_arena);
    ASSERT(gfx_family_idx != VK_QUEUE_FAMILY_IGNORED);

    VkDevice vk_device = create_vk_device(vk_instance, vk_phys_device, gfx_family_idx);
    volkLoadDevice(vk_device);

    VkQueue vk_queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(vk_device, gfx_family_idx, 0, &vk_queue);

    VkFormat swapchain_fmt = get_swapchain_fmt(vk_phys_device, vk_surface, &program_arena);

    VkSurfaceCapabilitiesKHR surface_caps = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_phys_device, vk_surface, &surface_caps);

    u32 surface_width = surface_caps.currentExtent.width;
    u32 surface_height = surface_caps.currentExtent.height;

    VkSwapchainKHR vk_swapchain = create_vk_swapchain(vk_device, vk_surface, swapchain_fmt, gfx_family_idx, surface_width, surface_height);

    u32 swapchain_image_count = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &swapchain_image_count, nullptr));

    Slice<VkImage> swapchain_images = arena_push_array<VkImage>(&program_arena, swapchain_image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &swapchain_image_count, swapchain_images.array));

    Slice<VkImageView> swapchain_image_views = arena_push_array<VkImageView>(&program_arena, swapchain_image_count);
    for (u32 i = 0; i < swapchain_image_count; ++i)
    {
        VkImageViewCreateInfo create_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        create_info.image = swapchain_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = swapchain_fmt;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(vk_device, &create_info, nullptr, &swapchain_image_views[i]));
    }

    VkRenderPass vk_render_pass = create_vk_fullframe_renderpass(vk_device, swapchain_fmt);

    Slice<VkFramebuffer> swapchain_framebuffers = arena_push_array<VkFramebuffer>(&program_arena, swapchain_image_count);
    for (u32 i = 0; i < swapchain_image_count; ++i)
    {
        VkFramebufferCreateInfo create_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        create_info.renderPass = vk_render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = &swapchain_image_views[i];
        create_info.width = surface_width;
        create_info.height = surface_height;
        create_info.layers = 1;

        VK_CHECK(vkCreateFramebuffer(vk_device, &create_info, nullptr, &swapchain_framebuffers[i]));
    }

    VkSemaphore img_acq_semaphore = VK_NULL_HANDLE;
    VkSemaphore img_rel_semaphore = VK_NULL_HANDLE;
    {
        VkSemaphoreCreateInfo create_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(vk_device, &create_info, nullptr, &img_acq_semaphore));
        VK_CHECK(vkCreateSemaphore(vk_device, &create_info, nullptr, &img_rel_semaphore));
        VK_ASSERT_VALID(img_acq_semaphore);
        VK_ASSERT_VALID(img_rel_semaphore);
    }

    shader_compiler_init();

    char shader_path[MAX_PATH] = "\0";
    strcpy(shader_path, root_dir);
    strcat(shader_path, "src/shaders/triangle.vert.glsl");
    VkShaderModule vert_shader = compile_shader(vk_device, Shader_Stage::vertex, String{shader_path, MAX_PATH}, &program_arena);

    shader_path[0] = '\0';
    strcpy(shader_path, root_dir);
    strcat(shader_path, "src/shaders/triangle.frag.glsl");
    VkShaderModule frag_shader = compile_shader(vk_device, Shader_Stage::fragment, String{shader_path, MAX_PATH}, &program_arena);

    // TODO(): Configure later
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;

    VkPipelineLayout triangle_layout = VK_NULL_HANDLE;
    {
        VkPushConstantRange push_constants;
        push_constants.offset = 0;
        // push_constants.size = sizeof(glm::mat4);
        push_constants.size = sizeof(Matrix4);
        push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkPipelineLayoutCreateInfo create_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        create_info.pPushConstantRanges = &push_constants;
        create_info.pushConstantRangeCount = 1;
    
        VK_CHECK(vkCreatePipelineLayout(vk_device, &create_info, nullptr, &triangle_layout));
    }

    VkPipeline triangle_pipeline = VK_NULL_HANDLE;
    {
        VkGraphicsPipelineCreateInfo pipe_create_info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

        VkPipelineShaderStageCreateInfo shader_stages[2] = {};
        shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stages[0].module = vert_shader;
        shader_stages[0].pName = "main";
        shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stages[1].module = frag_shader;
        shader_stages[1].pName = "main";
        pipe_create_info.stageCount = ARRAYSIZE(shader_stages);
        pipe_create_info.pStages = shader_stages;

        VkPipelineVertexInputStateCreateInfo vertex_input = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        pipe_create_info.pVertexInputState = &vertex_input;

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipe_create_info.pInputAssemblyState = &input_assembly;

        VkPipelineViewportStateCreateInfo viewport_state = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;
        pipe_create_info.pViewportState = &viewport_state;

        VkPipelineRasterizationStateCreateInfo raster_state = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster_state.lineWidth = 1.f;
        raster_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
        raster_state.cullMode = VK_CULL_MODE_NONE;
        raster_state.polygonMode = VK_POLYGON_MODE_FILL;
        pipe_create_info.pRasterizationState = &raster_state;

        VkPipelineMultisampleStateCreateInfo multisample_state = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        pipe_create_info.pMultisampleState = &multisample_state;

        VkPipelineDepthStencilStateCreateInfo ds_state = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        pipe_create_info.pDepthStencilState = &ds_state;

        VkPipelineColorBlendAttachmentState color_attachment_state = {};
        color_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo color_blend_state = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        color_blend_state.attachmentCount = 1;
        color_blend_state.pAttachments = &color_attachment_state;
        pipe_create_info.pColorBlendState = &color_blend_state;

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_info = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic_state_info.dynamicStateCount = ARRAYSIZE(dynamic_states);
        dynamic_state_info.pDynamicStates = dynamic_states;
        pipe_create_info.pDynamicState = &dynamic_state_info;

        pipe_create_info.layout = triangle_layout;
        pipe_create_info.renderPass = vk_render_pass;

        VK_CHECK(vkCreateGraphicsPipelines(vk_device, pipeline_cache, 1, &pipe_create_info, nullptr, &triangle_pipeline));
    }

    VkCommandPool vk_cmd_pool = VK_NULL_HANDLE;
    {
        VkCommandPoolCreateInfo create_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        create_info.queueFamilyIndex = gfx_family_idx;

        VK_CHECK(vkCreateCommandPool(vk_device, &create_info, nullptr, &vk_cmd_pool));
    }

    VkCommandBuffer vk_cmd_buffer = VK_NULL_HANDLE;
    {
        VkCommandBufferAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate_info.commandPool = vk_cmd_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(vk_device, &allocate_info, &vk_cmd_buffer));
    }

    u64 frame_idx = 0;
    Vector3 camera_pos;

    while (!platform_window_closing(main_window_handle))
    {
        Input_Events input_events = {};
        platform_pump_events(platform_app, main_window_handle, &input_events);

        u32 img_idx = 0;
        u64 const max_timeout = ~0ull;
        VkResult get_next_img_result = vkAcquireNextImageKHR(vk_device, vk_swapchain, max_timeout, img_acq_semaphore, VK_NULL_HANDLE, &img_idx);
        VK_CHECK(get_next_img_result);

        VkCommandPoolResetFlags pool_reset_flags = 0;
        VK_CHECK(vkResetCommandPool(vk_device, vk_cmd_pool, pool_reset_flags));

        VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(vk_cmd_buffer, &begin_info));

        VkImageMemoryBarrier render_begin_barrier = create_image_barrier(
            swapchain_images[img_idx],
            VK_ACCESS_NONE,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        vkCmdPipelineBarrier(
            vk_cmd_buffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, 0, 0, 0, 1,
            &render_begin_barrier);

        VkClearColorValue color = {{48.f / 255.f, 10.f / 255.f, 36.f / 255.f, 1.f}};
        VkClearValue clear_color = {color};

        VkRenderPassBeginInfo pass_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        pass_begin_info.renderPass = vk_render_pass;
        pass_begin_info.framebuffer = swapchain_framebuffers[img_idx];
        pass_begin_info.renderArea.extent.width = surface_width;
        pass_begin_info.renderArea.extent.height = surface_height;
        pass_begin_info.clearValueCount = 1;
        pass_begin_info.pClearValues = &clear_color;

        vkCmdBeginRenderPass(vk_cmd_buffer, &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        // x and y are normally the upper left corner, but as we are negating the height
        // we are supposed to instead specify the lower left corner. Negating the height
        // negates the y coordinate in clip space, which saves us having to negate position.y
        // in the last step before rasterization (normally vertex shader for us).
        VkViewport viewport = {};
        viewport.x = 0.f;
        viewport.y = f32(surface_height);
        viewport.width = f32(surface_width);
        viewport.height = -f32(surface_height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = {surface_width, surface_height};

        vkCmdSetViewport(vk_cmd_buffer, 0, 1, &viewport);
        vkCmdSetScissor(vk_cmd_buffer, 0, 1, &scissor);

        vkCmdBindPipeline(vk_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, triangle_pipeline);

        if (input_events.key_down[Input_Key_Code::A]) camera_pos.x -= -0.1f;
        if (input_events.key_down[Input_Key_Code::D]) camera_pos.x += -0.1f;
        if (input_events.key_down[Input_Key_Code::S]) camera_pos.y -= -0.1f;
        if (input_events.key_down[Input_Key_Code::W]) camera_pos.y += -0.1f;

        Matrix4 view = matrix4_translate(vec3_add(camera_pos, Vector3{0.f, 0.f, -1.f}));
        Matrix4 projection = matrix4_perspective_RH(degree_to_rad(70.f), f32(surface_width) / f32(surface_height), 0.1f, 200.f);

        Matrix4 model = matrix4_rotate(Vector3{0.f, degree_to_rad(frame_idx) * 0.4f, 0.f});
        Matrix4 mesh_matrix = matrix4_mul(projection, matrix4_mul(view, model));

        vkCmdPushConstants(vk_cmd_buffer, triangle_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Matrix4), mesh_matrix.m);
        vkCmdDraw(vk_cmd_buffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(vk_cmd_buffer);

        VkImageMemoryBarrier render_end_barrier = create_image_barrier(
            swapchain_images[img_idx],
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_NONE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkCmdPipelineBarrier(
            vk_cmd_buffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, 0, 0, 0, 1,
            &render_end_barrier);

        VK_CHECK(vkEndCommandBuffer(vk_cmd_buffer));

        VkPipelineStageFlags submit_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &img_acq_semaphore;
        submit_info.pWaitDstStageMask = &submit_stage_mask;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &vk_cmd_buffer;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &img_rel_semaphore;

        vkQueueSubmit(vk_queue, 1, &submit_info, VK_NULL_HANDLE);

        VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &img_rel_semaphore;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &vk_swapchain;
        present_info.pImageIndices = &img_idx;

        VK_CHECK(vkQueuePresentKHR(vk_queue, &present_info));
        VK_CHECK(vkDeviceWaitIdle(vk_device));

        if (platform_did_window_size_change(main_window_handle) ||
            get_next_img_result == VK_ERROR_OUT_OF_DATE_KHR     ||
            get_next_img_result == VK_SUBOPTIMAL_KHR)
        {
            for (u32 i = 0; i < swapchain_image_count; ++i)
            {
                vkDestroyFramebuffer(vk_device, swapchain_framebuffers[i], nullptr);
            }

            for (u32 i = 0; i < swapchain_image_count; ++i)
            {
                vkDestroyImageView(vk_device, swapchain_image_views[i], nullptr);
            }

            vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);

            VkSurfaceCapabilitiesKHR new_surface_caps = {};
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_phys_device, vk_surface, &new_surface_caps);
            surface_width = new_surface_caps.currentExtent.width;
            surface_height = new_surface_caps.currentExtent.height;

            LOG("Window size changed: w %u h %u. Recreating the swapchain.", surface_width, surface_height);

            vk_swapchain = create_vk_swapchain(vk_device, vk_surface, swapchain_fmt, gfx_family_idx, surface_width, surface_height);

            u32 new_swapchain_image_count = 0;
            VK_CHECK(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &new_swapchain_image_count, nullptr));
            ASSERT(new_swapchain_image_count == swapchain_image_count); // if these changed we'd need to be able to re-allocate the memory used below.

            VK_CHECK(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &swapchain_image_count, swapchain_images.array));

            for (u32 i = 0; i < swapchain_image_count; ++i)
            {
                VkImageViewCreateInfo create_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                create_info.image = swapchain_images[i];
                create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                create_info.format = swapchain_fmt;
                create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                create_info.subresourceRange.levelCount = 1;
                create_info.subresourceRange.layerCount = 1;

                VK_CHECK(vkCreateImageView(vk_device, &create_info, nullptr, &swapchain_image_views[i]));
            }

            for (u32 i = 0; i < swapchain_image_count; ++i)
            {
                VkFramebufferCreateInfo create_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                create_info.renderPass = vk_render_pass;
                create_info.attachmentCount = 1;
                create_info.pAttachments = &swapchain_image_views[i];
                create_info.width = surface_width;
                create_info.height = surface_height;
                create_info.layers = 1;

                VK_CHECK(vkCreateFramebuffer(vk_device, &create_info, nullptr, &swapchain_framebuffers[i]));
            }
        }

        ++frame_idx;
    }

    shader_compiler_shutdown();

    vkDestroyShaderModule(vk_device, vert_shader, nullptr);
    vkDestroyShaderModule(vk_device, frag_shader, nullptr);

    vkDestroyPipelineLayout(vk_device, triangle_layout, nullptr);
    vkDestroyPipeline(vk_device, triangle_pipeline, nullptr);

    vkDestroySemaphore(vk_device, img_acq_semaphore, nullptr);
    vkDestroySemaphore(vk_device, img_rel_semaphore, nullptr);

    vkDestroyRenderPass(vk_device, vk_render_pass, nullptr);
    vkDestroyCommandPool(vk_device, vk_cmd_pool, nullptr);

    for (u32 i = 0; i < swapchain_image_count; ++i)
    {
        vkDestroyFramebuffer(vk_device, swapchain_framebuffers[i], nullptr);
    }

    for (u32 i = 0; i < swapchain_image_count; ++i)
    {
        vkDestroyImageView(vk_device, swapchain_image_views[i], nullptr);
    }

    vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
    vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);

    vkDestroyDevice(vk_device, nullptr);

#if DEBUG_BUILD
    vkDestroyDebugUtilsMessengerEXT(vk_instance, vk_debug_utils.messenger, nullptr);
    vkDestroyDebugReportCallbackEXT(vk_instance, vk_debug_utils.report_callback, nullptr);
#endif // DEBUG_BUILD

    vkDestroyInstance(vk_instance, nullptr);

    platform_destroy_window(main_window_handle);
    platform_destroy_app(platform_app);

    LOG("Engine shutdown complete.");

    return 0;
}
#endif