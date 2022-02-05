#include "shader_compiler.h"
#include "core.h"
#include "glslang_c_interface.h"
#include "volk/resource_limits_c.h"
#include "vk.h"

struct Buffer
{
	u8* data = nullptr;
	u32 size = 0;

	bool is_valid() const { return data != nullptr; }
	void free() { delete data; }
};

Buffer load_file(char const* path)
{
	DEFER
	{
		log_last_windows_error();
	};

	HANDLE file_handle = CreateFileA(
		path,
		GENERIC_READ,
		FILE_SHARE_READ,
		0,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		0);

	DEFER
	{
		CloseHandle(file_handle);
	};

	if (file_handle == INVALID_HANDLE_VALUE)
	{
		ASSERT_FAILED_MSG("Failed to open file %", path);
		return Buffer();
	}

	u32 file_size;
	{
		DWORD file_query;
		GetFileSize(file_handle, &file_query);
		file_size = file_query;
	}

	if (file_size == INVALID_FILE_SIZE)
	{
		ASSERT_FAILED_MSG("Failed to get size of file %s", path);
		return Buffer();
	}

	u8* data = new u8[file_size];
	
	DWORD num_bytes_read = 0;
	if (!ReadFile(file_handle, data, file_size, &num_bytes_read, nullptr))
	{
		ASSERT_FAILED_MSG("Failed to read data from file %s", path);
		delete data;
		return Buffer();
	}

	if (num_bytes_read != file_size)
	{
		ASSERT_FAILED_MSG("Expected to read %llu bytes, but got %d bytes!", file_size, num_bytes_read);
		delete data;
		return Buffer();
	}

	return Buffer { data, num_bytes_read };
}

glslang_stage_t map_stage(Shader_Stage::Enum val)
{
	switch (val)
	{
	case Shader_Stage::vertex:   return GLSLANG_STAGE_VERTEX;
	case Shader_Stage::fragment: return GLSLANG_STAGE_FRAGMENT;
	case Shader_Stage::compute:  return GLSLANG_STAGE_COMPUTE;
	default:
	{
		ASSERT_FAILED_MSG("Attempted to map from unknown shader stage");
		return GLSLANG_STAGE_COUNT;
	}
	}
}

glslang_target_client_version_t map_version(u32 val)
{
	switch (val)
	{
	case VK_API_VERSION_1_2: return GLSLANG_TARGET_VULKAN_1_2;
	default:
	{
		ASSERT_FAILED_MSG("Attempted to map from unknown vulkan version");
		return GLSLANG_TARGET_CLIENT_VERSION_COUNT;
	}
	}
}

VkShaderModule compile_shader(VkDevice vk_device, Shader_Stage::Enum stage, char const* src_path)
{
	Buffer shader_code = load_file(src_path);
	if (!shader_code.is_valid())
	{
		LOG("Failed to load shader from %s", src_path);
	}

	DEFER{ shader_code.free(); };

	glslang_input_t input = {};
	input.language = GLSLANG_SOURCE_GLSL;
	input.stage = map_stage(stage);
	input.client = GLSLANG_CLIENT_VULKAN;
	input.client_version = map_version(C_TARGET_VK_VERSION);
	input.target_language = GLSLANG_TARGET_SPV;
	input.target_language_version = GLSLANG_TARGET_SPV_1_1;
	input.default_version = 100;
	input.default_profile = GLSLANG_CORE_PROFILE;
	input.force_default_version_and_profile = false;
	input.forward_compatible = false;
	input.messages = GLSLANG_MSG_DEFAULT_BIT;
	input.resource = glslang_default_resource();
	input.code = reinterpret_cast<char const*>(shader_code.data);

	glslang_shader_t* shader = glslang_shader_create(&input);
	DEFER{ glslang_shader_delete(shader); };

	auto log_shader_info = [](glslang_shader_t* shader)
	{
		LOG("Shader Compilation Error:");
		LOG("%s", glslang_shader_get_info_log(shader));
		LOG("%s", glslang_shader_get_info_debug_log(shader));
	};

	if (!glslang_shader_preprocess(shader, &input))
	{
		log_shader_info(shader);
		ASSERT_FAILED_MSG("Failed pre-processing shader %s", src_path);
		return VK_NULL_HANDLE;
	}

	if (!glslang_shader_parse(shader, &input))
	{
		log_shader_info(shader);
		ASSERT_FAILED_MSG("Failed parsing shader %s", src_path);
		return VK_NULL_HANDLE;
	}

	glslang_program_t* program = glslang_program_create();
	DEFER{ glslang_program_delete(program); };

	glslang_program_add_shader(program, shader);

	if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
	{
		log_shader_info(shader);
		ASSERT_FAILED_MSG("Failed linking shader %s", src_path);
		return VK_NULL_HANDLE;
	}

	glslang_program_SPIRV_generate(program, input.stage);
	u32 byte_code_size = glslang_program_SPIRV_get_size(program);
	u32* byte_code = new u32[byte_code_size];
	DEFER{ delete byte_code; };

	glslang_program_SPIRV_get(program, byte_code);

	{
		char const* spirv_messages = glslang_program_SPIRV_get_messages(program);
		if (spirv_messages)
		{
			LOG("SPIRV generation messages:\n%s", spirv_messages);
		}
	}

	VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = byte_code_size;
	create_info.pCode = byte_code;

	VkShaderModule vk_shader = VK_NULL_HANDLE;
	VK_CHECK(vkCreateShaderModule(vk_device, &create_info, nullptr, &vk_shader));

	return vk_shader;
}

void shader_compiler_init()
{
	glslang_initialize_process();
}

void shader_compiler_shutdown()
{
	glslang_finalize_process();
}