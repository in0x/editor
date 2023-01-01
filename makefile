src_dir := ./src
src_files := $(shell find ${src_dir} -name '*.cpp' -or -name '*.mm' -or -name '*.c')
header_only_files := %volk.c

src_files := $(filter-out ${header_only_files},${src_files})

build_dir := editor.app/Contents/MacOS
obj_files := $(src_files:${src_dir}/%=$(build_dir)/%.o)
dep_files = $(obj_files:%.o=%.deps)

vk_ver = 1.3.236.0
linker_flags := -g -framework foundation -framework cocoa -framework quartzcore -framework metal -L $$VULKAN_SDK/${vk_ver}/macOS/lib -lshaderc_combined

compile_flags := -std=c++17 -Wall -g
include_flags := -D VK_USE_PLATFORM_MACOS_MVK \
-D VK_USE_PLATFORM_METAL_EXT \
-D _DEBUG \
-I $$VULKAN_SDK/${vk_ver}/MacOS/include/glslang/Include \
-I $$VULKAN_SDK/${vk_ver}/MoltenVK/include \
-I $$HOMEBREW_PREFIX/opt/freetype/include/freetype2

all: ${build_dir}/editor

${build_dir}/editor: ${dep_files} ${obj_files}
	clang++ ${obj_files} -o ${build_dir}/editor ${linker_flags}

# on a clean build, we make the deps files because of the deps target
# on a build where the .cpp changes, the -MM thats also used when making 
# 	the .o remakes the .deps (we know how to make the .o because of the previous .deps, which asks for the .cpp)
# on a clean build where the .h changes, we also get a new .deps because the previous .deps gave us a dependency
#	on the .h file 

${build_dir}/%.cpp.deps: ${src_dir}/%.cpp
	clang++ -MT $(subst .deps,.o,$@) ${compile_flags} -MM -MF $@ $< ${include_flags}

${build_dir}/%.mm.deps: ${src_dir}/%.mm
	clang++ -MT $(subst .deps,.o,$@) ${compile_flags} -MM -MF $@ $< ${include_flags}

${build_dir}/%.c.deps: ${src_dir}/%.c
	clang++ -MT $(subst .deps,.o,$@) ${compile_flags} -MM -MF $@ $< ${include_flags}

ifneq ($(filter clean,$(MAKECMDGOALS)),clean)
-include ${dep_files}
endif

${build_dir}/%.cpp.o:
	clang++ -MT $@ ${compile_flags} -MF $(subst .o,.deps,$@) -MD -c $< -o $@ ${include_flags}

${build_dir}/%.c.o:
	clang++ -MT $@ ${compile_flags} -MF $(subst .o,.deps,$@) -MD -c $< -o $@ ${include_flags}

${build_dir}/%.mm.o:
	clang++ -MT $@ ${compile_flags} -MF $(subst .o,.deps,$@) -MD -c $< -o $@ ${include_flags}

.PHONY: clean
clean:
	rm -r editor.app/Contents/MacOS/*
