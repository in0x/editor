clang++ -std=c++17 -c -Wall -g src/main.cpp            -o editor.app/Contents/MacOS/main.o            -D VK_USE_PLATFORM_MACOS_MVK -D _DEBUG -I $VULKAN_SDK/1.3.204.1/MacOS/include/glslang/Include -I $VULKAN_SDK/1.3.204.1/MoltenVK/include
clang++ -std=c++17 -c -Wall -g src/core.cpp            -o editor.app/Contents/MacOS/core.o            -D VK_USE_PLATFORM_MACOS_MVK -D _DEBUG -I $VULKAN_SDK/1.3.204.1/MacOS/include/glslang/Include -I $VULKAN_SDK/1.3.204.1/MoltenVK/include
clang++ -std=c++17 -c -Wall -g src/memory.cpp          -o editor.app/Contents/MacOS/memory.o          -D VK_USE_PLATFORM_MACOS_MVK -D _DEBUG -I $VULKAN_SDK/1.3.204.1/MacOS/include/glslang/Include -I $VULKAN_SDK/1.3.204.1/MoltenVK/include
clang++ -std=c++17 -c -Wall -g src/osx.mm              -o editor.app/Contents/MacOS/osx.o             -D VK_USE_PLATFORM_MACOS_MVK -D _DEBUG -I $VULKAN_SDK/1.3.204.1/MacOS/include/glslang/Include -I $VULKAN_SDK/1.3.204.1/MoltenVK/include
clang++ -std=c++17 -c -Wall -g src/shader_compiler.cpp -o editor.app/Contents/MacOS/shader_compiler.o -D VK_USE_PLATFORM_MACOS_MVK -D _DEBUG -I $VULKAN_SDK/1.3.204.1/MacOS/include/glslang/Include -I $VULKAN_SDK/1.3.204.1/MoltenVK/include
clang++ -std=c++17 -c -Wall -g src/vk.cpp              -o editor.app/Contents/MacOS/vk.o              -D VK_USE_PLATFORM_MACOS_MVK -D _DEBUG -I $VULKAN_SDK/1.3.204.1/MacOS/include/glslang/Include -I $VULKAN_SDK/1.3.204.1/MoltenVK/include

clang++ -g editor.app/Contents/MacOS/main.o editor.app/Contents/MacOS/core.o editor.app/Contents/MacOS/memory.o editor.app/Contents/MacOS/osx.o editor.app/Contents/MacOS/shader_compiler.o editor.app/Contents/MacOS/vk.o -o editor.app/Contents/MacOS/editor -framework foundation -framework cocoa -framework quartzcore -framework metal -L $VULKAN_SDK/1.3.204.1/macOS/lib -lshaderc_combined