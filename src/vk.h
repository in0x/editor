#pragma once
#include "core.h"

#define VK_NO_PROTOTYPES
#include <MoltenVK/mvk_vulkan.h>

#include "3p/volk/volk.h"

constexpr u32 C_TARGET_VK_VERSION = VK_API_VERSION_1_3;

#define VK_CHECK(op)                                                                                    \
    do                                                                                                  \
    {                                                                                                   \
        VkResult __vkcr_result = (op);                                                                  \
        switch (__vkcr_result)                                                                          \
        {                                                                                               \
        case VK_SUCCESS:                                                                                \
        case VK_TIMEOUT:                                                                                \
        case VK_NOT_READY:                                                                              \
        case VK_SUBOPTIMAL_KHR:                                                                         \
        case VK_ERROR_OUT_OF_DATE_KHR:                                                                  \
				break;                                                                                  \
        case VK_ERROR_OUT_OF_HOST_MEMORY:                                                               \
            ASSERT_MSG(__vkcr_result == VK_SUCCESS, "Error OUT_OF_HOST_MEMORY (%d)", __vkcr_result);    \
            break;                                                                                      \
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:                                                             \
            ASSERT_MSG(__vkcr_result == VK_SUCCESS, "Error OUT_OF_DEVICE_MEMORY (%d)", __vkcr_result);  \
            break;                                                                                      \
        case VK_ERROR_INITIALIZATION_FAILED:                                                            \
            ASSERT_MSG(__vkcr_result == VK_SUCCESS, "Error INITIALIZATION_FAILED (%d)", __vkcr_result); \
            break;                                                                                      \
        case VK_ERROR_LAYER_NOT_PRESENT:                                                                \
            ASSERT_MSG(__vkcr_result == VK_SUCCESS, "Error LAYER_NOT_PRESENT (%d)", __vkcr_result);     \
            break;                                                                                      \
        case VK_ERROR_EXTENSION_NOT_PRESENT:                                                            \
            ASSERT_MSG(__vkcr_result == VK_SUCCESS, "Error EXTENSION_NOT_PRESENT (%d)", __vkcr_result); \
            break;                                                                                      \
        case VK_ERROR_INCOMPATIBLE_DRIVER:                                                              \
            ASSERT_MSG(__vkcr_result == VK_SUCCESS, "Error INCOMPATIBLE_DRIVER (%d)", __vkcr_result);   \
            break;                                                                                      \
        default:                                                                                        \
            ASSERT_MSG(__vkcr_result == VK_SUCCESS, "Error Code %d", __vkcr_result);                    \
            break;                                                                                      \
        }                                                                                               \
    } while (false)

#define VK_ASSERT_VALID(handle) ASSERT(handle != VK_NULL_HANDLE)