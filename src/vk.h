#pragma once
#include "core.h"
#include "volk/volk.h"

constexpr u32 C_TARGET_VK_VERSION = VK_API_VERSION_1_2;

#define VK_CHECK(op) \
	do { \
		VkResult result = op; \
		ASSERT_MSG(result == VK_SUCCESS, "Error code: %d", result); \
	} while (false)

#define VK_ASSERT_VALID(handle) ASSERT(handle != VK_NULL_HANDLE)