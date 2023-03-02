#include "core.h"
#include "context.h"
#include "mathlib.h"
#include "memory.h"
#include "platform.h"
#include "shader_compiler.h"
#include "timer.h"
#include "vk.h"

constexpr s64 MAX_FRAMES_IN_FLIGHT = 2;

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

u32 get_queue_family_index(VkPhysicalDevice phys_device, Context ctx, u32 queue_flags)
{
    ARENA_DEFER_CLEAR(ctx.tmp_bump);

    u32 queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_count, nullptr);

    Array<VkQueueFamilyProperties> queue_props = arena_push_array<VkQueueFamilyProperties>(ctx.tmp_bump, queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_count, queue_props.array);

    for (u32 i = 0; i < queue_count; ++i)
    {
        if (queue_props[i].queueFlags & queue_flags)
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
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#endif
    };

    create_info.ppEnabledExtensionNames = extensions;
    create_info.enabledExtensionCount = ARRAYSIZE(extensions);
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

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

static VkPhysicalDevice create_vk_physical_device(VkInstance vk_instance, VkSurfaceKHR vk_surface, Array<char const*> desired_extensions, Context ctx)
{
    ARENA_DEFER_CLEAR(ctx.tmp_bump);

    u32 phys_device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(vk_instance, &phys_device_count, nullptr));

    Array<VkPhysicalDevice> phys_devices = arena_push_array<VkPhysicalDevice>(ctx.tmp_bump, phys_device_count);

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

        u32 gfx_family_idx = get_queue_family_index(phys_devices[i], ctx, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
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

        Array<VkExtensionProperties> available_exts = arena_push_array_with_count<VkExtensionProperties>(ctx.tmp_bump, ext_count, ext_count);
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

static VkFormat get_swapchain_fmt(VkPhysicalDevice vk_phys_device, VkSurfaceKHR vk_surface, Context ctx)
{
    ARENA_DEFER_CLEAR(ctx.tmp_bump);

    VkFormat swapchain_fmt = VK_FORMAT_UNDEFINED;

    u32 fmt_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_phys_device, vk_surface, &fmt_count, nullptr));

    Array<VkSurfaceFormatKHR> fmts = arena_push_array_with_count<VkSurfaceFormatKHR>(ctx.tmp_bump, fmt_count, fmt_count);
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

static VkSwapchainKHR create_vk_swapchain(VkDevice vk_device, VkSurfaceKHR vk_surface, VkFormat swapchain_fmt, u32 gfx_family_idx, u32 numImages, u32 width, u32 height)
{
    VkSwapchainCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create_info.surface = vk_surface;
    create_info.minImageCount = numImages;
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

static VkRenderPass create_vk_fullframe_renderpass(VkDevice vk_device, VkFormat swapchain_fmt, VkFormat depth_fmt)
{
    VkAttachmentDescription cla = {};
    cla.format = swapchain_fmt;
    cla.samples = VK_SAMPLE_COUNT_1_BIT;
    cla.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    cla.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    cla.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    cla.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    cla.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    cla.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference cl_ref = {};
    cl_ref.attachment = 0;
    cl_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription dpa = {};
    dpa.format = depth_fmt;
    dpa.samples = VK_SAMPLE_COUNT_1_BIT;
    dpa.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    dpa.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    dpa.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    dpa.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    dpa.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    dpa.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference dp_ref = {};
    dp_ref.attachment = 1;
    dp_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &cl_ref;
    subpass.pDepthStencilAttachment = &dp_ref;

    VkAttachmentDescription attach_descs[] = { cla, dpa };

    VkRenderPassCreateInfo create_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    create_info.attachmentCount = ARRAYSIZE(attach_descs);
    create_info.pAttachments = attach_descs;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;

    VkRenderPass vk_render_pass = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(vk_device, &create_info, nullptr, &vk_render_pass));
    VK_ASSERT_VALID(vk_render_pass);
    return vk_render_pass;
}

void print_row(Mat4 const& m, u32 row)
{
    LOG("% 03.3f % 03.3f % 03.3f % 03.3f", m(row,0), m(row,1), m(row,2), m(row,3));
}

void print_matrix(Mat4 const& m)
{
    print_row(m, 0);
    print_row(m, 1);
    print_row(m, 2);
    print_row(m, 3);
    LOG("");
}

// todo:
// finish going through vulkan tutorial
// font rendering
// free cam
// window doesnt background
// VK_KHR_dynamic_rendering
// wait on uploads asynchronously
// allocator for vulkan memory
// model loading
// deffered render
// basic lighting
//  pbr
//  point / spot / directional
// HBAO
// natvis for Array<T>
// gpu text rendering
// imgui or custom ui
// physics
// animation
// sound

struct Cube_Geo
{
    //   5    6
    // 1    2
    //   4    7
    // 0    3
    // Vertex order when viewed head on: BL, TL, TR, BR
    constexpr static Vec3 vertices[] = 
    { 
        Vec3{-0.5f, -0.5f, -0.5f}, Vec3{-0.5f, 0.5f, -0.5f}, Vec3{0.5f, 0.5f, -0.5f}, Vec3{0.5f, -0.5f, -0.5f},
        Vec3{-0.5f, -0.5f, 0.5f}, Vec3{-0.5f, 0.5f, 0.5f}, Vec3{0.5f, 0.5f, 0.5f}, Vec3{0.5f, -0.5f, 0.5f},
    };

    constexpr u16 static indices[] =
    {
        0, 1, 3, 3, 1, 2,
        4, 5, 7, 7, 5, 6,
        1, 5, 2, 2, 5, 6,
        0, 4, 3, 3, 4, 7,
        0, 1, 4, 4, 1, 5,
        3, 2, 7, 7, 2, 6
    };

    // RGB Cube
    // https://www.pngitem.com/pimgs/m/592-5920896_rgb-color-model-cube-hd-png-download.png
    constexpr Vec3 static colors[] = 
    {
        Vec3{1.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 1.0f}, Vec3{1.0f, 1.0f, 1.0f}, Vec3{1.0f, 1.0f, 0.0f},
        Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}, Vec3{0.0f, 1.0f, 1.0f}, Vec3{0.0f, 1.0f, 0.0f},
    };    
};

static Option<u32> find_mem_idx(VkPhysicalDevice vk_physd, VkMemoryRequirements* requs, VkMemoryPropertyFlags flags)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(vk_physd, &mem_props);

    Option<u32> mem_idx;
    for (u32 i = 0; i < mem_props.memoryTypeCount; ++i)
    {
        bool matches_mem_type = requs->memoryTypeBits & (1 << i);
        bool matches_mem_props = (mem_props.memoryTypes[i].propertyFlags & flags) == flags; 
        if (matches_mem_type && matches_mem_props)
        {
            option_set(&mem_idx, i);
            break;
        }
    }
    return mem_idx;
}

struct GPU_Buffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};

struct GPU_Buffer_Params
{
    Array<u32> queue_families;
    VkDeviceSize size           = 0;
    VkBufferUsageFlags usage    = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_FLAG_BITS_MAX_ENUM;
};

GPU_Buffer create_gpu_buffer(VkDevice vk_device, VkPhysicalDevice vk_physd, GPU_Buffer_Params params)
{
    VkBufferCreateInfo ci = {
        .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size                  = params.size,
        .usage                 = params.usage,
        .sharingMode           = params.queue_families ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = params.queue_families ? (u32)params.queue_families.count : 1u,
        .pQueueFamilyIndices   = params.queue_families ? params.queue_families.array : nullptr,
    };

    GPU_Buffer result;
    VK_CHECK(vkCreateBuffer(vk_device, &ci, nullptr, &result.buffer));

    VkMemoryRequirements mem_requs;
    vkGetBufferMemoryRequirements(vk_device, result.buffer, &mem_requs);
    result.size = mem_requs.size;

    VkMemoryAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requs.size,
        .memoryTypeIndex = *find_mem_idx(vk_physd, &mem_requs, params.props)
    };
    
    VK_CHECK(vkAllocateMemory(vk_device, &ai, nullptr, &result.memory));
    vkBindBufferMemory(vk_device, result.buffer, result.memory, 0);
    return result;
}

void destroy_gpu_buffer(VkDevice vk_device, GPU_Buffer buffer)
{
    vkFreeMemory(vk_device, buffer.memory, nullptr);    
    vkDestroyBuffer(vk_device, buffer.buffer, nullptr);
}

struct GPU_Image
{
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct GPU_Image_Params
{
    Array<u32> queue_families;
    VkFormat fmt                    = VK_FORMAT_UNDEFINED;
    s64 width                       = 0;
    s64 height                      = 0;
    VkImageTiling tiling            = VK_IMAGE_TILING_MAX_ENUM;
    VkImageUsageFlags usage         = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;
    VkMemoryPropertyFlags mem_props = VK_MEMORY_PROPERTY_FLAG_BITS_MAX_ENUM;
};

static GPU_Image create_gpu_image(VkDevice vk_device, VkPhysicalDevice vk_physd, GPU_Image_Params params)
{
    VkImageCreateInfo ci = {
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType             = VK_IMAGE_TYPE_2D,
        .extent.width          = (u32)params.width,
        .extent.height         = (u32)params.height,
        .extent.depth          = 1,
        .mipLevels             = 1,
        .arrayLayers           = 1,
        .format                = params.fmt,
        .tiling                = params.tiling,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage                 = params.usage,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode           = params.queue_families ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = params.queue_families ? (u32)params.queue_families.count : 1u,
        .pQueueFamilyIndices   = params.queue_families ? params.queue_families.array : nullptr,
    };

    GPU_Image result;
    VK_CHECK(vkCreateImage(vk_device, &ci, nullptr, &result.image));

    VkMemoryRequirements mem_requs;
    vkGetImageMemoryRequirements(vk_device, result.image, &mem_requs);

    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mem_requs.size;
    ai.memoryTypeIndex = *find_mem_idx(vk_physd, &mem_requs, params.mem_props);

    VK_CHECK(vkAllocateMemory(vk_device, &ai, nullptr, &result.memory));

    vkBindImageMemory(vk_device, result.image, result.memory, 0);
    return result;
}

void destroy_gpu_image(VkDevice vk_device, GPU_Image img)
{
    vkFreeMemory(vk_device, img.memory, nullptr);
    vkDestroyImage(vk_device, img.image, nullptr);
}

static VkImageView create_image_view(VkDevice vk_device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo vci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange.aspectMask = aspect_flags,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1
    };

    VkImageView result;
    VK_CHECK(vkCreateImageView(vk_device, &vci, nullptr, &result));
    return result;
}

struct Depth_Buffer
{
    GPU_Image gpu_img;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat fmt = VK_FORMAT_UNDEFINED;
};

Depth_Buffer create_depth_buffer(VkDevice vk_device, VkPhysicalDevice vk_physd, s32 swawpchain_width, s32 swapchain_height)
{
    Depth_Buffer result;
    VkImageTiling desired_tiling = VK_IMAGE_TILING_OPTIMAL;
    VkFormat desired_fmts[] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT };
    for (VkFormat fmt : desired_fmts)
    {
        VkFormatProperties props = {};
        vkGetPhysicalDeviceFormatProperties(vk_physd, fmt, & props);
        VkFormatFeatureFlags& flags = (desired_tiling == VK_IMAGE_TILING_LINEAR) ? props.linearTilingFeatures : props.optimalTilingFeatures;
        if (flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            result.fmt = fmt;
            break;
        } 
    }

    result.gpu_img = create_gpu_image(vk_device, vk_physd, GPU_Image_Params {
        .fmt = result.fmt,
        .width = swawpchain_width,
        .height = swapchain_height,
        .tiling = desired_tiling,
        .mem_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    });

    result.view = create_image_view(vk_device, result.gpu_img.image, result.fmt, VK_IMAGE_ASPECT_DEPTH_BIT);
    return result;
}

void destroy_depth_buffer(VkDevice vk_device, Depth_Buffer db)
{
    destroy_gpu_image(vk_device, db.gpu_img);
    vkDestroyImageView(vk_device, db.view, nullptr);
}

struct Upload_Ctx
{
    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
};

struct Buffer_Upload
{
    GPU_Buffer staging_buffer;
    VkEvent upload_finished = VK_NULL_HANDLE;
};

static Upload_Ctx create_upload_context(VkDevice vk_device, u32 gfx_family_idx)
{
    Upload_Ctx result;

    VkCommandPoolCreateInfo cpai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = gfx_family_idx,
    };
    VK_CHECK(vkCreateCommandPool(vk_device, &cpai, nullptr, &result.cmd_pool));

    VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = result.cmd_pool,
        .commandBufferCount = 1
    };

    VK_CHECK(vkAllocateCommandBuffers(vk_device, &cbai, &result.cmd_buffer));

    return result;
};

static void destroy_upload_context(VkDevice vk_device, Upload_Ctx ctx)
{
    vkDestroyCommandPool(vk_device, ctx.cmd_pool, nullptr);    
}

static Buffer_Upload upload_to_buffer(VkDevice vk_device, VkPhysicalDevice vk_phys_device, 
    Upload_Ctx upload_ctx, GPU_Buffer dst_buffer, void* src, u64 num_bytes)
{
    Buffer_Upload result;
    result.staging_buffer = create_gpu_buffer(vk_device, vk_phys_device, GPU_Buffer_Params {
        .size  = num_bytes,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .props =  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    });

    void* dst = nullptr;
    vkMapMemory(vk_device, result.staging_buffer.memory, 0, num_bytes, 0, &dst);
    memcpy(dst, src, num_bytes);
    vkUnmapMemory(vk_device, result.staging_buffer.memory);

    VkBufferCopy copy_region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = num_bytes
    };
    vkCmdCopyBuffer(upload_ctx.cmd_buffer, result.staging_buffer.buffer, dst_buffer.buffer, 1, &copy_region);
    
    VkEventCreateInfo eci {VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};
    vkCreateEvent(vk_device, &eci, nullptr, &result.upload_finished);
    vkCmdSetEvent(upload_ctx.cmd_buffer, result.upload_finished, VK_PIPELINE_STAGE_NONE_KHR);
    return result;
}

static void release_upload_buffer(VkDevice vk_device, Buffer_Upload* buffer)
{
    ASSERT_MSG(vkGetEventStatus(vk_device, buffer->upload_finished) == VK_EVENT_SET,
        "Tried to release upload staging buffer before the upload has finished");

    vkDestroyEvent(vk_device, buffer->upload_finished, nullptr);
    destroy_gpu_buffer(vk_device, buffer->staging_buffer);
}

struct Model
{
    GPU_Buffer vertices;
    GPU_Buffer colors;
    GPU_Buffer indices;
    int num_vertices = 0;
    int num_colors   = 0;
    int num_indices  = 0;
};

struct Model_Upload
{
    Model model;
    Buffer_Upload vert_upload;
    Buffer_Upload col_upload;
    Buffer_Upload idx_upload;
};


struct Vk_Ctx
{
    VkPhysicalDevice phys_device = VK_NULL_HANDLE;
    VkDevice device              = VK_NULL_HANDLE;
    Upload_Ctx upload_ctx;
};

Model_Upload create_model(Vk_Ctx const* vk_ctx, Slice<Vec3> vertices, Slice<Vec3> colors, Slice<u16> indices)
{
    Model_Upload result;

    result.model.vertices = create_gpu_buffer(vk_ctx->device, vk_ctx->phys_device, GPU_Buffer_Params {
        .size = vertices.count * sizeof(Vec3), 
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    });
    result.model.num_vertices = vertices.count;

    result.model.colors = create_gpu_buffer(vk_ctx->device, vk_ctx->phys_device, GPU_Buffer_Params {
        .size = colors.count * sizeof(Vec3), 
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    });
    result.model.num_colors = colors.count;

    result.model.indices = create_gpu_buffer(vk_ctx->device, vk_ctx->phys_device, GPU_Buffer_Params {
        .size = indices.count * sizeof(u16),
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    });
    result.model.num_indices = indices.count;

    result.vert_upload = upload_to_buffer(vk_ctx->device, vk_ctx->phys_device, 
        vk_ctx->upload_ctx, result.model.vertices, (void*)vertices.array, vertices.count * sizeof(Vec3));

    result.col_upload = upload_to_buffer(vk_ctx->device, vk_ctx->phys_device, 
        vk_ctx->upload_ctx, result.model.colors, (void*)colors.array, colors.count * sizeof(Vec3));

    result.idx_upload = upload_to_buffer(vk_ctx->device, vk_ctx->phys_device,
        vk_ctx->upload_ctx, result.model.indices, (void*)indices.array, indices.count * sizeof(u16));

    return result;
}

void destroy_model(Vk_Ctx const* vk_ctx, Model* model)
{
    destroy_gpu_buffer(vk_ctx->device, model->vertices);
    destroy_gpu_buffer(vk_ctx->device, model->colors);
    destroy_gpu_buffer(vk_ctx->device, model->indices);
    zero_struct(model);
}

void finish_model_creation(Vk_Ctx const* vk_ctx, Model_Upload* upload)
{
    release_upload_buffer(vk_ctx->device, &upload->vert_upload);
    release_upload_buffer(vk_ctx->device, &upload->col_upload);
    release_upload_buffer(vk_ctx->device, &upload->idx_upload);
}

#if PLATFORM_WIN32
INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
#else
int main(int argc, char** argv)
{
    // We want two allocators, global and temporary lifetime.
    // This lets solve cases where permanent allocatiosn happen deeper in the stack then temp ones. If we use
    // one allocator, the permanent alloc blocks the temp ones from being freed. Example: loading a mesh, helper
    // allocations vs permanent mesh data alloc. With two, the helpers go on the temp (freed per scope or once
    // per frame -> then we can have two for rendering last frame async), the mesh data goes on the permanent stack.
    Arena program_lifetime_allocator = arena_allocate(1024 * 1024 * 10); // Give ourselves 10 MiB of memory to place global lifetime allocs.
    Arena temporary_lifetime_allocator = arena_allocate(1024 * 1024);  // And 1 MiB for temporary allocations (lifetime < 1 frame).
    DEFER  {
        arena_free(&program_lifetime_allocator);
        arena_free(&temporary_lifetime_allocator); };
    Context ctx;
    ctx.bump = &program_lifetime_allocator;
    ctx.tmp_bump = &temporary_lifetime_allocator;

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

    test_mat4_mul();

    VK_CHECK(volkInitialize());

    Vk_Ctx vk_ctx;

    VkInstance vk_instance = create_vk_instance();
    volkLoadInstanceOnly(vk_instance);

    Screen_Props main_screen_props = platform_get_main_window_props();
    Create_Window_Params window_params = {};
    window_params.width = 400;
    window_params.height = 400;
    window_params.x = main_screen_props.width - window_params.width;
    window_params.y = 300;
    // window_params.class_name = L"editor_window_class";
    window_params.title = "Editor";
    Platform_Window main_window_handle = platform_create_window(platform_app, window_params);

#if DEBUG_BUILD
    Vulkan_Debug_Utils vk_debug_utils = vk_create_debug_utils(vk_instance);
#endif // DEBUG_BUILD

    VkSurfaceKHR vk_surface = create_vk_surface(vk_instance, platform_window_get_raw_handle(main_window_handle));

    char const* phys_device_exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    Array<char const*> phys_device_ext_slice{&phys_device_exts, 1};
    VkPhysicalDevice vk_phys_device = create_vk_physical_device(vk_instance, vk_surface, phys_device_ext_slice, ctx);
    vk_ctx.phys_device = vk_phys_device;

    u32 const gfx_family_idx = get_queue_family_index(vk_phys_device, ctx, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
    ASSERT(gfx_family_idx != VK_QUEUE_FAMILY_IGNORED);

    VkDevice vk_device = create_vk_device(vk_instance, vk_phys_device, gfx_family_idx);
    vk_ctx.device = vk_device;
    volkLoadDevice(vk_device);

    VkQueue gfx_queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(vk_device, gfx_family_idx, 0, &gfx_queue);

    VkFormat swapchain_fmt = get_swapchain_fmt(vk_phys_device, vk_surface, ctx);

    VkSurfaceCapabilitiesKHR surface_caps = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_phys_device, vk_surface, &surface_caps);

    u32 surface_width  = surface_caps.currentExtent.width;
    u32 surface_height = surface_caps.currentExtent.height;
    u32 surface_count  = MAX_FRAMES_IN_FLIGHT + surface_caps.minImageCount;
    surface_count = clamp(surface_count, surface_caps.maxImageCount, surface_caps.maxImageCount);
    VkSwapchainKHR vk_swapchain = create_vk_swapchain(vk_device, vk_surface, swapchain_fmt, gfx_family_idx, surface_count, surface_width, surface_height);

    u32 swapchain_image_count = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &swapchain_image_count, nullptr));

    Array<VkImage> swapchain_images = arena_push_array<VkImage>(ctx.bump, swapchain_image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &swapchain_image_count, swapchain_images.array));

    Array<VkImageView> swapchain_image_views = arena_push_array<VkImageView>(ctx.bump, swapchain_image_count);
    for (u32 i = 0; i < swapchain_image_count; ++i)
    {
        swapchain_image_views[i] = create_image_view(vk_device, swapchain_images[i], swapchain_fmt, VK_IMAGE_ASPECT_COLOR_BIT);    
    }

    Depth_Buffer depth_buffer = create_depth_buffer(vk_device, vk_phys_device, surface_width, surface_height);

    VkRenderPass vk_render_pass = create_vk_fullframe_renderpass(vk_device, swapchain_fmt, depth_buffer.fmt);

    Array<VkFramebuffer> swapchain_framebuffers = arena_push_array<VkFramebuffer>(ctx.bump, swapchain_image_count);
    for (u32 i = 0; i < swapchain_image_count; ++i)
    {
        VkImageView attachments[] = { swapchain_image_views[i], depth_buffer.view };
        VkFramebufferCreateInfo create_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        create_info.renderPass = vk_render_pass;
        create_info.attachmentCount = ARRAYSIZE(attachments);
        create_info.pAttachments = attachments;
        create_info.width = surface_width;
        create_info.height = surface_height;
        create_info.layers = 1;

        VK_CHECK(vkCreateFramebuffer(vk_device, &create_info, nullptr, &swapchain_framebuffers[i]));
    }

    VkSemaphore img_acq_semaphore[MAX_FRAMES_IN_FLIGHT] = {};
    VkSemaphore img_rel_semaphore[MAX_FRAMES_IN_FLIGHT] = {};
    {
        VkSemaphoreCreateInfo create_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (s64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            VK_CHECK(vkCreateSemaphore(vk_device, &create_info, nullptr, &img_acq_semaphore[i]));
            VK_ASSERT_VALID(img_acq_semaphore[i]);
        }
        for (s64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            VK_CHECK(vkCreateSemaphore(vk_device, &create_info, nullptr, &img_rel_semaphore[i]));
            VK_ASSERT_VALID(img_rel_semaphore[i]);
        }
    }

    VkFence end_of_frame_fences[MAX_FRAMES_IN_FLIGHT] = {};
    {
        VkFenceCreateInfo create_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (s64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            VK_CHECK(vkCreateFence(vk_device, &create_info, nullptr, &end_of_frame_fences[i]));
            VK_ASSERT_VALID(end_of_frame_fences[i]);
        }
    }

    shader_compiler_init();

    char shader_path[MAX_PATH] = "\0";
    strcpy(shader_path, root_dir);
    strcat(shader_path, "src/shaders/basic.vert.glsl");
    VkShaderModule vert_shader = compile_shader(vk_device, Shader_Stage::vertex, String{shader_path, MAX_PATH}, &ctx);

    shader_path[0] = '\0';
    strcpy(shader_path, root_dir);
    strcat(shader_path, "src/shaders/triangle.frag.glsl");
    VkShaderModule frag_shader = compile_shader(vk_device, Shader_Stage::fragment, String{shader_path, MAX_PATH}, &ctx);

    // TODO(): Configure later
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;

    VkPipelineLayout triangle_layout = VK_NULL_HANDLE;
    {
        VkPushConstantRange push_constants;
        push_constants.offset = 0;
        // push_constants.size = sizeof(glm::mat4);
        push_constants.size = sizeof(Mat4);
        push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkPipelineLayoutCreateInfo create_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        create_info.pPushConstantRanges = &push_constants;
        create_info.pushConstantRangeCount = 1;

        VK_CHECK(vkCreatePipelineLayout(vk_device, &create_info, nullptr, &triangle_layout));
    }

    enum Buffer_T { Pos = 0, Col, Idx, Count, Vert_T_Cnt = 2 };
    VkPipeline triangle_pipeline = VK_NULL_HANDLE;
    {
        // TODO(): Automate with shader reflection
        VkVertexInputBindingDescription vert_binds[Buffer_T::Vert_T_Cnt];
        vert_binds[Buffer_T::Pos].binding = Buffer_T::Pos;
        vert_binds[Buffer_T::Col].binding = Buffer_T::Col;
        vert_binds[Buffer_T::Col].stride = sizeof(Vec3);
        vert_binds[Buffer_T::Pos].stride = sizeof(Vec3);
        vert_binds[Buffer_T::Pos].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vert_binds[Buffer_T::Col].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vert_attrs[Buffer_T::Vert_T_Cnt];
        vert_attrs[Buffer_T::Pos].binding = Buffer_T::Pos;
        vert_attrs[Buffer_T::Col].binding = Buffer_T::Col;
        vert_attrs[Buffer_T::Pos].location = Buffer_T::Pos;
        vert_attrs[Buffer_T::Col].location = Buffer_T::Col;
        vert_attrs[Buffer_T::Pos].format = VK_FORMAT_R32G32B32_SFLOAT;
        vert_attrs[Buffer_T::Col].format = VK_FORMAT_R32G32B32_SFLOAT;
        vert_attrs[Buffer_T::Pos].offset = 0;
        vert_attrs[Buffer_T::Col].offset = 0;

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
        vertex_input.vertexBindingDescriptionCount = Buffer_T::Vert_T_Cnt;
        vertex_input.vertexAttributeDescriptionCount = Buffer_T::Vert_T_Cnt;
        vertex_input.pVertexBindingDescriptions = vert_binds;
        vertex_input.pVertexAttributeDescriptions = vert_attrs;
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

        VkPipelineDepthStencilStateCreateInfo depth_stencil {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f, 
            .stencilTestEnable = VK_FALSE,
        };
        pipe_create_info.pDepthStencilState = &depth_stencil;

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

    VkCommandPool gfx_cmd_pool = VK_NULL_HANDLE;
    {
        VkCommandPoolCreateInfo create_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        create_info.queueFamilyIndex = gfx_family_idx;
        VK_CHECK(vkCreateCommandPool(vk_device, &create_info, nullptr, &gfx_cmd_pool));
    }

    VkCommandBuffer vk_cmd_buffers[MAX_FRAMES_IN_FLIGHT] = {};
    {
        VkCommandBufferAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate_info.commandPool = gfx_cmd_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        VK_CHECK(vkAllocateCommandBuffers(vk_device, &allocate_info, &vk_cmd_buffers[0]));
    }

    vk_ctx.upload_ctx = create_upload_context(vk_device, gfx_family_idx);

    Model cube_model;
    Model cube_model_2;
    {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        VK_CHECK(vkBeginCommandBuffer(vk_ctx.upload_ctx.cmd_buffer, &begin_info));
            
        Model_Upload model_upload = create_model(
            &vk_ctx,
            Slice<Vec3> { Cube_Geo::vertices },
            Slice<Vec3> { Cube_Geo::colors },
            Slice<u16>  { Cube_Geo::indices }
        );
        cube_model = model_upload.model;

        Model_Upload model_upload_2 = create_model(
            &vk_ctx,
            Slice<Vec3> { Cube_Geo::vertices },
            Slice<Vec3> { Cube_Geo::colors },
            Slice<u16>  { Cube_Geo::indices }
        );
        cube_model_2 = model_upload_2.model;

        vkEndCommandBuffer(vk_ctx.upload_ctx.cmd_buffer);

        VkSubmitInfo sbi = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &vk_ctx.upload_ctx.cmd_buffer
        };

        vkQueueSubmit(gfx_queue, 1, &sbi, VK_NULL_HANDLE);
        vkQueueWaitIdle(gfx_queue);
        finish_model_creation(&vk_ctx, &model_upload);
        finish_model_creation(&vk_ctx, &model_upload_2);
    }
    
    Timer frame_timer = make_timer();
    s64 frame_count = 0;

    Vec3 azi_zen_zoom;

    f64 s_since_step = 0;
    f64 const step_len_s = 16.6 / 1000.0; // step physics at 60 hz
    while (!platform_window_closing(main_window_handle))
    {
        s64 frame_idx = frame_count % MAX_FRAMES_IN_FLIGHT;
        f64 dt_s = tick_ms(&frame_timer);
        // LOG("Frame %d[%d] Time: %f ms", frame_count, frame_idx, dt_s);

        Input_State const* input_state = platform_pump_events(platform_app, main_window_handle);

        if (input_state->key_down[Input_Key_Code::ESC])
        {
            break;
        }

        u64 const max_timeout = ~0ull;

        vkWaitForFences(vk_device, 1, &end_of_frame_fences[frame_idx], VK_TRUE, max_timeout);
        vkResetFences(vk_device, 1, &end_of_frame_fences[frame_idx]);

        u32 img_idx = 0;
        VkResult get_next_img_result = vkAcquireNextImageKHR(vk_device, vk_swapchain, max_timeout, img_acq_semaphore[frame_idx], VK_NULL_HANDLE, &img_idx);
        VK_CHECK(get_next_img_result);

        VkCommandBuffer frame_cmds = vk_cmd_buffers[frame_idx];
        vkResetCommandBuffer(frame_cmds, 0);

        VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(frame_cmds, &begin_info));

        VkImageMemoryBarrier render_begin_barrier = create_image_barrier(
            swapchain_images[img_idx],
            VK_ACCESS_NONE,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        vkCmdPipelineBarrier(
            frame_cmds,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, 0, 0, 0, 1,
            &render_begin_barrier);

        VkClearValue clear_colors[] = {
            VkClearValue {
                .color = {
                    {0.f, 0.f, 0.f, 1.0f}
                    // { 48.f / 255.f, 10.f / 255.f, 36.f / 255.f, 1.f }
            }},
            VkClearValue {
                .depthStencil = {
                    .depth = 1.f, .stencil = 0
            }}
        };

        VkRenderPassBeginInfo pass_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        pass_begin_info.renderPass = vk_render_pass;
        pass_begin_info.framebuffer = swapchain_framebuffers[img_idx];
        pass_begin_info.renderArea.extent.width = surface_width;
        pass_begin_info.renderArea.extent.height = surface_height;
        pass_begin_info.clearValueCount = ARRAYSIZE(clear_colors);
        pass_begin_info.pClearValues = clear_colors;

        vkCmdBeginRenderPass(frame_cmds, &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

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
        vkCmdSetViewport(frame_cmds, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = {surface_width, surface_height};
        vkCmdSetScissor(frame_cmds, 0, 1, &scissor);

        vkCmdBindPipeline(frame_cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, triangle_pipeline);

        s_since_step += dt_s;
        Vec3 prev_azi_zen;
        while (s_since_step >= step_len_s)
        {
            // https://www.mbsoftworks.sk/tutorials/opengl4/026-camera-pt3-orbit-camera/
            prev_azi_zen = azi_zen_zoom;

            f32 rot_deg = degree_to_rad(0.1f) * step_len_s;
            f32 rot_y = 0.f;
            f32 rot_x = 0.f;
            if (input_state->key_down[Input_Key_Code::A]) rot_y -= rot_deg;
            if (input_state->key_down[Input_Key_Code::D]) rot_y += rot_deg;
            if (input_state->key_down[Input_Key_Code::S]) rot_x -= rot_deg;
            if (input_state->key_down[Input_Key_Code::W]) rot_x += rot_deg;

            if (rot_y != 0)
            {
                f32 full_circle = 2.f * Pi;
                azi_zen_zoom.x += rot_y;
                azi_zen_zoom.x = fmodf(azi_zen_zoom.x, full_circle); // Prevent the value growing above 360 deg
                if (azi_zen_zoom.x < 0.f)
                    azi_zen_zoom.x = full_circle + azi_zen_zoom.x; // Clamp negative values 0 < x < 360
            }

            if (rot_x != 0)
            {
                // Stop rotation at slightly below 45 degrees to prevent becoming colinear with cardinal axis
                f32 quarter_circle = Pi / 2.0f - 0.001f;
                azi_zen_zoom.y += rot_x;
                azi_zen_zoom.y = clamp(azi_zen_zoom.y, -quarter_circle, quarter_circle);
            }

            if (input_state->scroll_wheel)
            {
                f32 sign = input_state->scroll_wheel > 0.f ? 1.f : -1.f;
                f32 zoom = sign * 0.005f * step_len_s;
                azi_zen_zoom.z += zoom;
            }

            s_since_step -= step_len_s;
        }
        azi_zen_zoom = lerp(azi_zen_zoom, prev_azi_zen, s_since_step / step_len_s);
        azi_zen_zoom.z = clamp(azi_zen_zoom.z, 2.f, 10.f);

        Vec3 cam_pos;
        {
            f32 sin_azi = sinf(azi_zen_zoom.x);
            f32 cos_azi = cosf(azi_zen_zoom.x);
            f32 sin_zen = sinf(azi_zen_zoom.y);
            f32 cos_zen = cosf(azi_zen_zoom.y);
            cam_pos = {
                azi_zen_zoom.z * cos_zen * cos_azi,
                azi_zen_zoom.z * sin_zen,
                azi_zen_zoom.z * cos_zen * sin_azi,
            };
        }

        Mat4 view = mat4_look_at(cam_pos, vec3_zero(), Vec3{0.f, 1.f, 0.f});
        
        Mat4 projection = mat4_perspective(degree_to_rad(70.f), f32(surface_width) / f32(surface_height), 0.1f, 200.f);
        Mat4 model = mat4_identity();
        Mat4 mesh_matrix = mat4_mul(projection, mat4_mul(view, model));

        VkDeviceSize buf_offsets[] = {0, 0};
        VkBuffer vert_bufs[] = { cube_model.vertices.buffer, cube_model.colors.buffer };
        vkCmdBindVertexBuffers(frame_cmds, 0, 2, vert_bufs, buf_offsets);
        vkCmdBindIndexBuffer(frame_cmds, cube_model.indices.buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdPushConstants(frame_cmds, triangle_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), mesh_matrix.m);
        vkCmdDrawIndexed(frame_cmds, cube_model.num_indices, 1, 0, 0, 0);

        model = mat4_translate(Vec3{0.f, 0.f, 2.f});
        mesh_matrix = mat4_mul(projection, mat4_mul(view, model));

        vert_bufs[0] = cube_model_2.vertices.buffer;
        vert_bufs[1] = cube_model_2.colors.buffer;
        vkCmdBindVertexBuffers(frame_cmds, 0, 2, vert_bufs, buf_offsets);
        vkCmdBindIndexBuffer(frame_cmds, cube_model_2.indices.buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdPushConstants(frame_cmds, triangle_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), mesh_matrix.m);
        vkCmdDrawIndexed(frame_cmds, cube_model_2.num_indices, 1, 0, 0, 0);


        vkCmdEndRenderPass(frame_cmds);

        VkImageMemoryBarrier render_end_barrier = create_image_barrier(
            swapchain_images[img_idx],
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_NONE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkCmdPipelineBarrier(
            frame_cmds,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, 0, 0, 0, 1,
            &render_end_barrier);

        VK_CHECK(vkEndCommandBuffer(frame_cmds));

        VkPipelineStageFlags submit_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &img_acq_semaphore[frame_idx];
        submit_info.pWaitDstStageMask = &submit_stage_mask;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &frame_cmds;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &img_rel_semaphore[frame_idx];

        vkQueueSubmit(gfx_queue, 1, &submit_info, end_of_frame_fences[frame_idx]);

        VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &img_rel_semaphore[frame_idx];
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &vk_swapchain;
        present_info.pImageIndices = &img_idx;

        VK_CHECK(vkQueuePresentKHR(gfx_queue, &present_info));

        if (platform_did_window_size_change(main_window_handle) ||
            get_next_img_result == VK_ERROR_OUT_OF_DATE_KHR     ||
            get_next_img_result == VK_SUBOPTIMAL_KHR)
        {
            vkDeviceWaitIdle(vk_device);

            destroy_depth_buffer(vk_device, depth_buffer);

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

            depth_buffer = create_depth_buffer(vk_device, vk_phys_device, surface_width, surface_height);
            vk_swapchain = create_vk_swapchain(vk_device, vk_surface, swapchain_fmt, gfx_family_idx, surface_count, surface_width, surface_height);

            u32 new_swapchain_image_count = 0;
            VK_CHECK(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &new_swapchain_image_count, nullptr));
            ASSERT(new_swapchain_image_count == swapchain_image_count); // if these changed we'd need to be able to re-allocate the memory used below.

            VK_CHECK(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &swapchain_image_count, swapchain_images.array));

            for (u32 i = 0; i < swapchain_image_count; ++i)
            {
                swapchain_image_views[i] = create_image_view(vk_device, swapchain_images[i], swapchain_fmt, VK_IMAGE_ASPECT_COLOR_BIT);    
            }

            for (u32 i = 0; i < swapchain_image_count; ++i)
            {
                VkImageView attachments[] = { swapchain_image_views[i], depth_buffer.view };
                VkFramebufferCreateInfo create_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                create_info.renderPass = vk_render_pass;
                create_info.attachmentCount = ARRAYSIZE(attachments);
                create_info.pAttachments = attachments;
                create_info.width = surface_width;
                create_info.height = surface_height;
                create_info.layers = 1;

                VK_CHECK(vkCreateFramebuffer(vk_device, &create_info, nullptr, &swapchain_framebuffers[i]));
            }
        }

        ++frame_count;
    }

    VK_CHECK(vkDeviceWaitIdle(vk_device));

    shader_compiler_shutdown();

    vkDestroyShaderModule(vk_device, vert_shader, nullptr);
    vkDestroyShaderModule(vk_device, frag_shader, nullptr);

    vkDestroyPipelineLayout(vk_device, triangle_layout, nullptr);
    vkDestroyPipeline(vk_device, triangle_pipeline, nullptr);

    for (s64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        vkDestroyFence(vk_device, end_of_frame_fences[i], nullptr);

    for (s64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        vkDestroySemaphore(vk_device, img_acq_semaphore[i], nullptr);

    for (s64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        vkDestroySemaphore(vk_device, img_rel_semaphore[i], nullptr);

    vkDestroyRenderPass(vk_device, vk_render_pass, nullptr);
    vkDestroyCommandPool(vk_device, gfx_cmd_pool, nullptr); // destroying the command pool also destroys its commandbuffers.
    
    destroy_model(&vk_ctx, &cube_model);

    destroy_upload_context(vk_device, vk_ctx.upload_ctx);

    destroy_depth_buffer(vk_device, depth_buffer);

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
