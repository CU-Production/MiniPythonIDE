cmake_minimum_required(VERSION 3.25)
project(MiniPythonIDE)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif ()

# Option to enable/disable debugger support (default: ON)
option(ENABLE_DEBUGGER "Enable Python debugger support" ON)

add_subdirectory(3rd_party)

# Source files
set(MINIPYTHONIDE_SOURCES
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
        3rd_party/imgui/imgui_impl_sdl3.cpp
        3rd_party/ImGuiColorTextEdit/TextEditor.cpp
        3rd_party/pocketpy/pocketpy.c)

# Add debugger sources if enabled
if(ENABLE_DEBUGGER)
    list(APPEND MINIPYTHONIDE_SOURCES
        src/debugger.cpp
        src/debugger.h
        src/pocketpy_debugger_internal.h)
endif()

add_executable(MiniPythonIDE ${MINIPYTHONIDE_SOURCES})
target_include_directories(MiniPythonIDE PRIVATE
        ${CMAKE_BINARY_DIR}
        3rd_party/imgui
        3rd_party/ImGuiColorTextEdit
        3rd_party/pocketpy
        3rd_party/tinyfiledialogs)

# PocketPy threading is now enabled with C11 atomics support
if(MSVC)
    target_compile_options(MiniPythonIDE PRIVATE "/bigobj")
    target_compile_options(MiniPythonIDE PRIVATE "/utf-8")
    target_compile_options(MiniPythonIDE PRIVATE "/experimental:c11atomics")
endif()

# Add ENABLE_DEBUGGER definition if debugger is enabled
if(ENABLE_DEBUGGER)
    target_compile_definitions(MiniPythonIDE PRIVATE ENABLE_DEBUGGER=1)
    message(STATUS "Python debugger support: ENABLED")
else()
    message(STATUS "Python debugger support: DISABLED")
endif()
target_link_libraries(MiniPythonIDE PRIVATE
        freetype
        SDL3::SDL3)

# Link Windows socket library for PocketPy
if(WIN32)
    target_link_libraries(MiniPythonIDE PRIVATE ws2_32)
endif()
add_custom_command(
        TARGET MiniPythonIDE POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ARGS ${CMAKE_CURRENT_SOURCE_DIR}/test.py ${CMAKE_CURRENT_BINARY_DIR}/test.py
        COMMAND ${CMAKE_COMMAND} -E copy
        ARGS ${CMAKE_CURRENT_SOURCE_DIR}/test_debug_variables.py ${CMAKE_CURRENT_BINARY_DIR}/test_debug_variables.py
        COMMAND ${CMAKE_COMMAND} -E copy
        ARGS ${CMAKE_CURRENT_SOURCE_DIR}/test_segmented_display.py ${CMAKE_CURRENT_BINARY_DIR}/test_segmented_display.py
)
