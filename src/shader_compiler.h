#pragma once

struct VkDevice_T;
struct VkShaderModule_T;

struct Shader_Stage
{
	enum Enum 
	{
		vertex,
		fragment,
		compute
	};
};

VkShaderModule_T* compile_shader(VkDevice_T* vk_device, Shader_Stage::Enum stage, char const* src_path);

void shader_compiler_init();
void shader_compiler_shutdown();