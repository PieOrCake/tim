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
    src/ChatLinks.cpp
    src/GW2API.cpp
    ${IMGUI_SOURCES}
)

# Header files
set(HEADERS
    include/nexus/Nexus.h
    src/ChatManager.h
    src/WhisperHook.h
    src/ChatLinks.h
    src/GW2API.h
)

# Pack pre-edited Nyan Cat frames into a C header at build time
file(GLOB NYAN_FRAME_PNGS "${CMAKE_SOURCE_DIR}/assets/nyan/nyan_frame_*.png")
add_custom_command(
    OUTPUT  ${CMAKE_SOURCE_DIR}/src/nyancat_frames.h
    COMMAND bash ${CMAKE_SOURCE_DIR}/cmake/gen_nyan_frames.sh
            ${CMAKE_SOURCE_DIR}/assets/nyan
            ${CMAKE_SOURCE_DIR}/src/nyancat_frames.h
    DEPENDS ${NYAN_FRAME_PNGS}
            ${CMAKE_SOURCE_DIR}/cmake/gen_nyan_frames.sh
    COMMENT "Generating src/nyancat_frames.h from assets/nyan/"
    VERBATIM
)
add_custom_target(nyancat_frames_h
    DEPENDS ${CMAKE_SOURCE_DIR}/src/nyancat_frames.h)

# Create shared library (DLL)
add_library(TyrianIM SHARED ${SOURCES} ${HEADERS})

add_dependencies(TyrianIM nyancat_frames_h)

# Force Windows DLL extension
set_target_properties(TyrianIM PROPERTIES
    SUFFIX ".dll"
    PREFIX ""
)

# Link Windows libraries
target_link_libraries(TyrianIM
    gdi32
    winmm
    winhttp
)

# Module definition file for clean DLL export
target_link_options(TyrianIM PRIVATE -Wl,--out-implib,TyrianIM.lib -Wl,--strip-all)
target_sources(TyrianIM PRIVATE ${CMAKE_SOURCE_DIR}/TyrianIM.def)

# Output directory
set_target_properties(TyrianIM PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
