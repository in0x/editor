#pragma once

struct VkDevice_T;
struct VkShaderModule_T;

VkShaderModule_T* load_shader(VkDevice_T* vk_device, char const* path);