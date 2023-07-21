cmake_minimum_required(VERSION 3.25)
project(MiniPythonIDE)

set(CMAKE_CXX_STANDARD 17)

if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif ()

set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "")
add_subdirectory(3rd_party/freetype)

add_executable(MiniPythonIDE
        main.cpp
        3rd_party/imgui/imgui.cpp
        3rd_party/imgui/imgui_draw.cpp
        3rd_party/imgui/imgui_tables.cpp
        3rd_party/imgui/imgui_widgets.cpp
        3rd_party/imgui/misc/freetype/imgui_freetype.cpp
        3rd_party/ImGuiColorTextEdit/TextEditor.cpp)
target_include_directories(MiniPythonIDE PRIVATE
        3rd_party/imgui
        3rd_party/ImGuiColorTextEdit
        3rd_party/pocketpy
        3rd_party/sokol)
target_link_libraries(MiniPythonIDE PRIVATE
        freetype)
add_custom_command(
        TARGET MiniPythonIDE POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ARGS ${CMAKE_CURRENT_SOURCE_DIR}/test.py ${CMAKE_CURRENT_BINARY_DIR}/test.py
)
