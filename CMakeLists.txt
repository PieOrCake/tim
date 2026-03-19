cmake_minimum_required(VERSION 3.20)
project(TyrianIM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release build for optimized, stripped output
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")

# Cross-compilation: use cmake/mingw-toolchain.cmake via:
#   cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-toolchain.cmake ..

# Static linking to avoid Windows DLL dependencies
set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -DWIN32_LEAN_AND_MEAN")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/lib)
include_directories(${CMAKE_SOURCE_DIR}/lib/imgui)
include_directories(${CMAKE_SOURCE_DIR}/lib/imgui/backends)

# ImGui source files
set(IMGUI_SOURCES
    lib/imgui/imgui.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/imgui_widgets.cpp
)

# Source files
set(SOURCES
    src/dllmain.cpp
    src/ChatManager.cpp
    src/WhisperHook.cpp
    ${IMGUI_SOURCES}
)

# Header files
set(HEADERS
    include/nexus/Nexus.h
    src/ChatManager.h
    src/WhisperHook.h
)

# Create shared library (DLL)
add_library(TyrianIM SHARED ${SOURCES} ${HEADERS})

# Force Windows DLL extension
set_target_properties(TyrianIM PROPERTIES
    SUFFIX ".dll"
    PREFIX ""
)

# Link Windows libraries
target_link_libraries(TyrianIM
    gdi32
    winmm
)

# Module definition file for clean DLL export
target_link_options(TyrianIM PRIVATE -Wl,--out-implib,TyrianIM.lib -Wl,--strip-all)
target_sources(TyrianIM PRIVATE ${CMAKE_SOURCE_DIR}/TyrianIM.def)

# Output directory
set_target_properties(TyrianIM PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
