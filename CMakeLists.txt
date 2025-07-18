cmake_minimum_required(VERSION 3.25)
project(MiniPythonIDE)

set(CMAKE_CXX_STANDARD 17)

if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif ()

configure_file(${CMAKE_CURRENT_LIST_DIR}/cmake/config_app.h.cmake ${CMAKE_BINARY_DIR}/config_app.h)

add_subdirectory(3rd_party)

add_executable(MiniPythonIDE
        src/main.cpp
        src/editor.cpp
        src/editor.h
        3rd_party/tinyfiledialogs/tinyfiledialogs.c
        3rd_party/imgui/imgui.cpp
        3rd_party/imgui/imgui_draw.cpp
        3rd_party/imgui/imgui_tables.cpp
        3rd_party/imgui/imgui_widgets.cpp
        3rd_party/imgui/misc/freetype/imgui_freetype.cpp
        3rd_party/imgui/imgui_impl_sdlgpu3.cpp
        3rd_party/imgui/imgui_impl_sdl3.cpp)
target_include_directories(MiniPythonIDE PRIVATE
        ${CMAKE_BINARY_DIR}
        3rd_party/imgui
        3rd_party/ImGuiColorTextEdit
        3rd_party/pocketpy
        3rd_party/zep/include
        3rd_party/tinyfiledialogs)
target_link_libraries(MiniPythonIDE PRIVATE
        freetype
        SDL3::SDL3)
target_compile_definitions(MiniPythonIDE PRIVATE
        ZEP_FEATURE_CPP_FILE_SYSTEM
        ZEP_USE_SDL
        ZEP_SINGLE_HEADER=1)
add_custom_command(
        TARGET MiniPythonIDE POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ARGS ${CMAKE_CURRENT_SOURCE_DIR}/test.py ${CMAKE_CURRENT_BINARY_DIR}/test.py
)
