#pragma once
#include "core.h"

struct VkDevice_T;
struct VkShaderModule_T;

struct Arena;

struct Shader_Stage
{
	enum Enum 
	{
		vertex,
		fragment,
		compute
	};
};

VkShaderModule_T* compile_shader(VkDevice_T* vk_device, Shader_Stage::Enum stage, String src_path, Arena* arena);

void shader_compiler_init();
void shader_compiler_shutdown();