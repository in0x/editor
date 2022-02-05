#include "shader_compiler.h"
#include "core.h"
#include "vk.h"

VkShaderModule load_shader(VkDevice vk_device, char const* path)
{
	// TODO: actually compile the shader using glslang

	HANDLE shader_file = CreateFileA(
		path,
		GENERIC_READ,
		FILE_SHARE_READ,
		0,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		0);

	if (shader_file == INVALID_HANDLE_VALUE)
	{
		ASSERT_FAILED_MSG("Failed to open shader file %", path);
		return VK_NULL_HANDLE;
	}

	DEFER
	{
		CloseHandle(shader_file);
	};

	u32 file_size;
	{
		DWORD file_query;
		GetFileSize(shader_file, &file_query);
		file_size = file_query;
	}

	if (file_size == INVALID_FILE_SIZE)
	{
		ASSERT_FAILED_MSG("Failed to get size of shader file %s", path);
		return VK_NULL_HANDLE;
	}

	u8* data = new u8[file_size];
	DEFER
	{
		delete data;
	};

	DWORD num_bytes_read = 0;
	if (!ReadFile(shader_file, data, file_size, &num_bytes_read, nullptr))
	{
		ASSERT_FAILED_MSG("Failed to read data from shader file %s", path);
		return VK_NULL_HANDLE;
	}

	if (num_bytes_read != file_size)
	{
		ASSERT_FAILED_MSG("Expected to read %llu bytes, but got %d bytes!", file_size, num_bytes_read);
		return VK_NULL_HANDLE;
	}

	VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = num_bytes_read;
	create_info.pCode = reinterpret_cast<u32*>(data);

	VkShaderModule shader = VK_NULL_HANDLE;
	VK_CHECK(vkCreateShaderModule(vk_device, &create_info, nullptr, &shader));

	return shader;
}
