#pragma once
#include "core.h"
#include "volk/volk.h"

#define VK_CHECK(op) \
	do { \
		VkResult result = op; \
		ASSERT_MSG(result == VK_SUCCESS, "Error code: %d", result); \
	} while (false)

#define VK_ASSERT_VALID(handle) ASSERT(handle != VK_NULL_HANDLE)