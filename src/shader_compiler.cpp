#include "shader_compiler.h"
#include "core.h"
#include "glslang_c_interface.h"
#include "memory.h"
#include "platform.h"
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

Slice load_file(String file_path, Arena* arena)
{
	File_Handle file_handle = open_file(file_path);
	DEFER { log_last_platform_error(); };
	DEFER { close_file(file_handle); };

	if (!is_file_valid(file_handle))
	{
		ASSERT_FAILED_MSG("Failed to open shader file %s", file_path.buffer);
	    return Slice {};
	}

	Option<u64> file_size_result = get_file_size(file_handle);
	if (!file_size_result.has_value)
	{
		ASSERT_FAILED_MSG("Failed to read size of shader file %s", file_path.buffer);
		return Slice {};
	}
    
	u64 content_size = file_size_result.value + 1; // allocate an extra byte for the null-terminator.
	Slice shader_data = arena_push(arena, content_size);

    Option<u64> read_result = read_file(file_handle, shader_data, file_size_result.value);
    if (!read_result.has_value)
    {
        return Slice {};
    }

	if (read_result.value != file_size_result.value)
	{
		ASSERT_FAILED_MSG("Expected to read %llu bytes, but got %d bytes!", file_size_result.value, read_result.value);
        return Slice {};
	}

	shader_data[read_result.value] = '\0'; // the read data is good, set the null terminator now
	return shader_data;
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

VkShaderModule compile_shader(VkDevice vk_device, Shader_Stage::Enum stage, String src_path, Arena* arena)
{
	ARENA_DEFER_CLEAR(arena);

	Slice shader_code = load_file(src_path, arena);
	if (!shader_code.is_valid())
	{
		LOG("Failed to load shader from %s", src_path.buffer);
        return VK_NULL_HANDLE;
    }

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
	input.code = reinterpret_cast<char const*>(shader_code.buffer);

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
	ArraySlice<u32> byte_code = arena_push_array<u32>(arena, byte_code_size);

	glslang_program_SPIRV_get(program, byte_code.m_array);

	{
		char const* spirv_messages = glslang_program_SPIRV_get_messages(program);
		if (spirv_messages)
		{
			LOG("SPIRV generation messages:\n%s", spirv_messages);
		}
	}

	VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = byte_code_size * sizeof(u32);
	create_info.pCode = byte_code.m_array;

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