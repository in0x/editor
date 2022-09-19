#pragma once
#include "core.h"

#define VK_NO_PROTOTYPES
#include <MoltenVK/mvk_vulkan.h>

#include "volk/volk.h"

constexpr u32 C_TARGET_VK_VERSION = VK_API_VERSION_1_2;

#define VK_CHECK(op) \
	do { \
		VkResult result = op; \
		switch (result)				\
		{							\
			case VK_SUCCESS: break; \
			case VK_TIMEOUT: break; \
			case VK_NOT_READY: break; \
			case VK_SUBOPTIMAL_KHR: break; \
			case VK_ERROR_OUT_OF_HOST_MEMORY:    ASSERT_MSG(result == VK_SUCCESS, "Error OUT_OF_HOST_MEMORY (%d)", result);    \
				break; \
			case VK_ERROR_OUT_OF_DEVICE_MEMORY:  ASSERT_MSG(result == VK_SUCCESS, "Error OUT_OF_DEVICE_MEMORY (%d)", result);  \
				break; \
			case VK_ERROR_INITIALIZATION_FAILED: ASSERT_MSG(result == VK_SUCCESS, "Error INITIALIZATION_FAILED (%d)", result); \
				break; \
			case VK_ERROR_LAYER_NOT_PRESENT:	 ASSERT_MSG(result == VK_SUCCESS, "Error LAYER_NOT_PRESENT (%d)", result); 	   \
				break; \
			case VK_ERROR_EXTENSION_NOT_PRESENT: ASSERT_MSG(result == VK_SUCCESS, "Error EXTENSION_NOT_PRESENT (%d)", result); \
				break; \
			case VK_ERROR_INCOMPATIBLE_DRIVER:	 ASSERT_MSG(result == VK_SUCCESS, "Error INCOMPATIBLE_DRIVER (%d)", result);   \
				break; \
			default: ASSERT_MSG(result == VK_SUCCESS, "Error Code %d", result); 											   \
				break; \
		} \
	} while (false) \

#define VK_ASSERT_VALID(handle) ASSERT(handle != VK_NULL_HANDLE)