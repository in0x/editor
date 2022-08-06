clang++ -std=c++17 src/main.cpp src/core.cpp  -o editor.app/Contents/MacOS/editor -framework foundation -framework cocoa -framework quartzcore -D VK_USE_PLATFORM_MACOS_MVK -D _DEBUG
