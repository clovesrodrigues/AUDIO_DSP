cmake_minimum_required(VERSION 3.14)

project(CV_GUI
    VERSION 0.1.0
    DESCRIPTION "Reusable GUI infrastructure for AUDIO_DSP"
    LANGUAGES CXX
)

set(CV_GUI_VST3SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../backends/vst3sdk" CACHE PATH "Path to the bundled VST3 SDK")
set(CV_GUI_IMGUI_DIR "${CMAKE_CURRENT_LIST_DIR}/../backends/imgui" CACHE PATH "Path to the bundled Dear ImGui")
option(CV_GUI_DEVELOPMENT "Build CV_GUI with VST3 DEVELOPMENT diagnostics enabled" ON)

if(NOT EXISTS "${CV_GUI_VST3SDK_DIR}/pluginterfaces/gui/iplugview.h")
    message(FATAL_ERROR "VST3 SDK not found at ${CV_GUI_VST3SDK_DIR}")
endif()

if(NOT EXISTS "${CV_GUI_IMGUI_DIR}/imgui.h")
    message(FATAL_ERROR "Dear ImGui not found at ${CV_GUI_IMGUI_DIR}")
endif()

set(CV_GUI_COMPAT_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/compat_include")
file(MAKE_DIRECTORY "${CV_GUI_COMPAT_INCLUDE_DIR}/public.sdk")
if(NOT EXISTS "${CV_GUI_COMPAT_INCLUDE_DIR}/public.sdk/source")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink
        "${CV_GUI_VST3SDK_DIR}/public.sdk"
        "${CV_GUI_COMPAT_INCLUDE_DIR}/public.sdk/source")
endif()

add_library(cv_gui STATIC
    src/HostFallbackBackend.cpp
    src/ImGuiBackend.cpp
    src/VST3ImGuiView.cpp
    src/imgui/ImGuiLayer.cpp
    src/vst3/VST3ParameterBridge.cpp
    ${CV_GUI_IMGUI_DIR}/imgui.cpp
    ${CV_GUI_IMGUI_DIR}/imgui_draw.cpp
    ${CV_GUI_IMGUI_DIR}/imgui_tables.cpp
    ${CV_GUI_IMGUI_DIR}/imgui_widgets.cpp
)

add_library(CV_GUI::cv_gui ALIAS cv_gui)

target_compile_features(cv_gui PUBLIC cxx_std_17)

target_include_directories(cv_gui
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CV_GUI_COMPAT_INCLUDE_DIR}
        ${CV_GUI_VST3SDK_DIR}
        ${CV_GUI_VST3SDK_DIR}/public.sdk
        ${CV_GUI_IMGUI_DIR}
)

target_compile_definitions(cv_gui
    PUBLIC
        CV_GUI_ENABLE_VST3=1
        CV_GUI_ENABLE_IMGUI=1
        $<$<BOOL:${CV_GUI_DEVELOPMENT}>:DEVELOPMENT=1;RELEASE=0>
        $<$<NOT:$<BOOL:${CV_GUI_DEVELOPMENT}>>:DEVELOPMENT=0;RELEASE=1>
    PRIVATE
        CV_GUI_BUILDING_LIBRARY=1
)

if(WIN32)
    target_sources(cv_gui
        PRIVATE
            src/platform/win32/Win32ChildWindow.cpp
            src/renderer/opengl3/OpenGL3Context.cpp
            src/renderer/opengl3/ImGuiOpenGL3Renderer.cpp
            ${CV_GUI_IMGUI_DIR}/backends/imgui_impl_win32.cpp
            ${CV_GUI_IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )

    target_compile_definitions(cv_gui
        PUBLIC
            CV_GUI_ENABLE_WIN32=1
            CV_GUI_ENABLE_OPENGL3=1
            WIN32_LEAN_AND_MEAN
            NOMINMAX
    )

    target_link_libraries(cv_gui
        PUBLIC
            opengl32
            gdi32
            user32
    )
endif()
