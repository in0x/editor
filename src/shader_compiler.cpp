#include "shader_compiler.h"
#include "core.h"
#include "glslang_c_interface.h"
#include "ResourceLimits.h"
#include "vk.h"

// Neither the vulkan SDK nor the latest glslang CI build
// seem to ship with this for some reason, so we just took
// it from the glslang repo.
namespace glslang {
	const TBuiltInResource DefaultTBuiltInResource = {
		/* .MaxLights = */ 32,
		/* .MaxClipPlanes = */ 6,
		/* .MaxTextureUnits = */ 32,
		/* .MaxTextureCoords = */ 32,
		/* .MaxVertexAttribs = */ 64,
		/* .MaxVertexUniformComponents = */ 4096,
		/* .MaxVaryingFloats = */ 64,
		/* .MaxVertexTextureImageUnits = */ 32,
		/* .MaxCombinedTextureImageUnits = */ 80,
		/* .MaxTextureImageUnits = */ 32,
		/* .MaxFragmentUniformComponents = */ 4096,
		/* .MaxDrawBuffers = */ 32,
		/* .MaxVertexUniformVectors = */ 128,
		/* .MaxVaryingVectors = */ 8,
		/* .MaxFragmentUniformVectors = */ 16,
		/* .MaxVertexOutputVectors = */ 16,
		/* .MaxFragmentInputVectors = */ 15,
		/* .MinProgramTexelOffset = */ -8,
		/* .MaxProgramTexelOffset = */ 7,
		/* .MaxClipDistances = */ 8,
		/* .MaxComputeWorkGroupCountX = */ 65535,
		/* .MaxComputeWorkGroupCountY = */ 65535,
		/* .MaxComputeWorkGroupCountZ = */ 65535,
		/* .MaxComputeWorkGroupSizeX = */ 1024,
		/* .MaxComputeWorkGroupSizeY = */ 1024,
		/* .MaxComputeWorkGroupSizeZ = */ 64,
		/* .MaxComputeUniformComponents = */ 1024,
		/* .MaxComputeTextureImageUnits = */ 16,
		/* .MaxComputeImageUniforms = */ 8,
		/* .MaxComputeAtomicCounters = */ 8,
		/* .MaxComputeAtomicCounterBuffers = */ 1,
		/* .MaxVaryingComponents = */ 60,
		/* .MaxVertexOutputComponents = */ 64,
		/* .MaxGeometryInputComponents = */ 64,
		/* .MaxGeometryOutputComponents = */ 128,
		/* .MaxFragmentInputComponents = */ 128,
		/* .MaxImageUnits = */ 8,
		/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
		/* .MaxCombinedShaderOutputResources = */ 8,
		/* .MaxImageSamples = */ 0,
		/* .MaxVertexImageUniforms = */ 0,
		/* .MaxTessControlImageUniforms = */ 0,
		/* .MaxTessEvaluationImageUniforms = */ 0,
		/* .MaxGeometryImageUniforms = */ 0,
		/* .MaxFragmentImageUniforms = */ 8,
		/* .MaxCombinedImageUniforms = */ 8,
		/* .MaxGeometryTextureImageUnits = */ 16,
		/* .MaxGeometryOutputVertices = */ 256,
		/* .MaxGeometryTotalOutputComponents = */ 1024,
		/* .MaxGeometryUniformComponents = */ 1024,
		/* .MaxGeometryVaryingComponents = */ 64,
		/* .MaxTessControlInputComponents = */ 128,
		/* .MaxTessControlOutputComponents = */ 128,
		/* .MaxTessControlTextureImageUnits = */ 16,
		/* .MaxTessControlUniformComponents = */ 1024,
		/* .MaxTessControlTotalOutputComponents = */ 4096,
		/* .MaxTessEvaluationInputComponents = */ 128,
		/* .MaxTessEvaluationOutputComponents = */ 128,
		/* .MaxTessEvaluationTextureImageUnits = */ 16,
		/* .MaxTessEvaluationUniformComponents = */ 1024,
		/* .MaxTessPatchComponents = */ 120,
		/* .MaxPatchVertices = */ 32,
		/* .MaxTessGenLevel = */ 64,
		/* .MaxViewports = */ 16,
		/* .MaxVertexAtomicCounters = */ 0,
		/* .MaxTessControlAtomicCounters = */ 0,
		/* .MaxTessEvaluationAtomicCounters = */ 0,
		/* .MaxGeometryAtomicCounters = */ 0,
		/* .MaxFragmentAtomicCounters = */ 8,
		/* .MaxCombinedAtomicCounters = */ 8,
		/* .MaxAtomicCounterBindings = */ 1,
		/* .MaxVertexAtomicCounterBuffers = */ 0,
		/* .MaxTessControlAtomicCounterBuffers = */ 0,
		/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
		/* .MaxGeometryAtomicCounterBuffers = */ 0,
		/* .MaxFragmentAtomicCounterBuffers = */ 1,
		/* .MaxCombinedAtomicCounterBuffers = */ 1,
		/* .MaxAtomicCounterBufferSize = */ 16384,
		/* .MaxTransformFeedbackBuffers = */ 4,
		/* .MaxTransformFeedbackInterleavedComponents = */ 64,
		/* .MaxCullDistances = */ 8,
		/* .MaxCombinedClipAndCullDistances = */ 8,
		/* .MaxSamples = */ 4,
		/* .maxMeshOutputVerticesNV = */ 256,
		/* .maxMeshOutputPrimitivesNV = */ 512,
		/* .maxMeshWorkGroupSizeX_NV = */ 32,
		/* .maxMeshWorkGroupSizeY_NV = */ 1,
		/* .maxMeshWorkGroupSizeZ_NV = */ 1,
		/* .maxTaskWorkGroupSizeX_NV = */ 32,
		/* .maxTaskWorkGroupSizeY_NV = */ 1,
		/* .maxTaskWorkGroupSizeZ_NV = */ 1,
		/* .maxMeshViewCountNV = */ 4,
		/* .maxDualSourceDrawBuffersEXT = */ 1,

		/* .limits = */ {
			/* .nonInductiveForLoops = */ 1,
			/* .whileLoops = */ 1,
			/* .doWhileLoops = */ 1,
			/* .generalUniformIndexing = */ 1,
			/* .generalAttributeMatrixVectorIndexing = */ 1,
			/* .generalVaryingIndexing = */ 1,
			/* .generalSamplerIndexing = */ 1,
			/* .generalVariableIndexing = */ 1,
			/* .generalConstantMatrixVectorIndexing = */ 1,
		} };
}

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
		ASSERT_FAILED_MSG("Failed to open file %s", path);
		return Buffer();
	}

	LARGE_INTEGER file_size;
	if (!GetFileSizeEx(file_handle, &file_size))
	{
		ASSERT_FAILED_MSG("Failed to get size of file %s", path);
		return Buffer();
	}
	
	u8* data = new u8[file_size.QuadPart + 1]; // alloc an extra byte to null-terminate the string
	memset(data, 0, file_size.QuadPart);
	
	DWORD num_bytes_read = 0;
	if (!ReadFile(file_handle, data, file_size.QuadPart, &num_bytes_read, nullptr))
	{
		ASSERT_FAILED_MSG("Failed to read data from file %s", path);
		delete data;
		return Buffer();
	}

	if (num_bytes_read != file_size.QuadPart)
	{
		ASSERT_FAILED_MSG("Expected to read %llu bytes, but got %d bytes!", file_size, num_bytes_read);
		delete data;
		return Buffer();
	}

	data[num_bytes_read] = '\0'; // the read data is good, set the null terminator now

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
	input.default_version = 400;
	input.default_profile = GLSLANG_CORE_PROFILE;
	input.force_default_version_and_profile = false;
	input.forward_compatible = false;
	input.messages = GLSLANG_MSG_DEFAULT_BIT;
	input.resource = (const glslang_resource_t*)&glslang::DefaultTBuiltInResource;
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
		LOG("%s", input.code);
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
	size_t byte_code_size = glslang_program_SPIRV_get_size(program);
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
	create_info.codeSize = byte_code_size * sizeof(u32);
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