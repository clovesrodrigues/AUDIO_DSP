cmake_minimum_required(VERSION 3.14.0)

project(gui_test_vst3
    VERSION 1.0.0.0
    DESCRIPTION "GUI Manager VST 3 Backend Test"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CV_GUI_TEST_BACKEND "Auto" CACHE STRING "GUI backend preference: Auto, ImGui, VSTGUI, or Native")
set_property(CACHE CV_GUI_TEST_BACKEND PROPERTY STRINGS Auto ImGui VSTGUI Native)

set(vst3sdk_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../backends/vst3sdk" CACHE PATH "Path to the bundled VST3 SDK")
set(imgui_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../backends/imgui" CACHE PATH "Path to bundled Dear ImGui")
set(gui_manager_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../gui_manager" CACHE PATH "Path to GUI Manager module")

if(NOT EXISTS "${vst3sdk_SOURCE_DIR}/pluginterfaces/base/funknown.h")
    message(FATAL_ERROR "Bundled VST3 SDK not found at ${vst3sdk_SOURCE_DIR}")
endif()
if(NOT EXISTS "${vst3sdk_SOURCE_DIR}/vstgui4/vstgui/vstgui.h")
    message(STATUS "VSTGUI backend probe: backends/vst3sdk/vstgui4/vstgui/vstgui.h was not found; VSTGUI requests will fall back to NativeHostBackend.")
endif()
if(NOT EXISTS "${imgui_SOURCE_DIR}/imgui.h")
    message(FATAL_ERROR "Bundled Dear ImGui not found at ${imgui_SOURCE_DIR}")
endif()

set(compat_include_dir "${PROJECT_BINARY_DIR}/compat_include")
file(MAKE_DIRECTORY "${compat_include_dir}/public.sdk")
if(NOT EXISTS "${compat_include_dir}/public.sdk/source")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink
        "${vst3sdk_SOURCE_DIR}/public.sdk"
        "${compat_include_dir}/public.sdk/source")
endif()

file(WRITE "${PROJECT_BINARY_DIR}/projectversion.h"
"#pragma once\n"
"#define MAJOR_VERSION_STR \"${PROJECT_VERSION_MAJOR}\"\n"
"#define SUB_VERSION_STR \"${PROJECT_VERSION_MINOR}\"\n"
"#define RELEASE_NUMBER_STR \"${PROJECT_VERSION_PATCH}\"\n"
"#define BUILD_NUMBER_STR \"${PROJECT_VERSION_TWEAK}\"\n"
"#define FULL_VERSION_STR \"${PROJECT_VERSION}\"\n"
"#define MAJOR_VERSION_INT ${PROJECT_VERSION_MAJOR}\n"
"#define SUB_VERSION_INT ${PROJECT_VERSION_MINOR}\n"
"#define RELEASE_NUMBER_INT ${PROJECT_VERSION_PATCH}\n"
"#define BUILD_NUMBER_INT ${PROJECT_VERSION_TWEAK}\n")

set(vst3_sdk_sources
    ${vst3sdk_SOURCE_DIR}/base/source/baseiids.cpp
    ${vst3sdk_SOURCE_DIR}/base/source/fbuffer.cpp
    ${vst3sdk_SOURCE_DIR}/base/source/fdebug.cpp
    ${vst3sdk_SOURCE_DIR}/base/source/fdynlib.cpp
    ${vst3sdk_SOURCE_DIR}/base/source/fobject.cpp
    ${vst3sdk_SOURCE_DIR}/base/source/fstreamer.cpp
    ${vst3sdk_SOURCE_DIR}/base/source/fstring.cpp
    ${vst3sdk_SOURCE_DIR}/base/source/timer.cpp
    ${vst3sdk_SOURCE_DIR}/base/source/updatehandler.cpp
    ${vst3sdk_SOURCE_DIR}/base/thread/source/fcondition.cpp
    ${vst3sdk_SOURCE_DIR}/base/thread/source/flock.cpp
    ${vst3sdk_SOURCE_DIR}/pluginterfaces/base/conststringtable.cpp
    ${vst3sdk_SOURCE_DIR}/pluginterfaces/base/coreiids.cpp
    ${vst3sdk_SOURCE_DIR}/pluginterfaces/base/funknown.cpp
    ${vst3sdk_SOURCE_DIR}/pluginterfaces/base/ustring.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/common/pluginview.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/main/linuxmain.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/main/moduleinit.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/main/pluginfactory.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/vst/vstaudioeffect.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/vst/vstbus.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/vst/vstcomponentbase.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/vst/vsteditcontroller.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/vst/vstparameters.cpp
)

set(imgui_sources
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_null.cpp
)

set(gui_manager_sources
    ${gui_manager_SOURCE_DIR}/source/GUIManager.cpp
    ${gui_manager_SOURCE_DIR}/source/ImGuiBackend.cpp
    ${gui_manager_SOURCE_DIR}/source/VSTGUIBackend.cpp
    ${gui_manager_SOURCE_DIR}/source/NativeHostBackend.cpp
)

add_library(gui_test_vst3 MODULE
    source/version.h
    source/TPcids.h
    source/TPprocessor.h
    source/TPprocessor.cpp
    source/TPcontroller.h
    source/TPcontroller.cpp
    source/TPentry.cpp
    ${gui_manager_sources}
    ${vst3_sdk_sources}
    ${imgui_sources}
)

target_include_directories(gui_test_vst3
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../..
        ${PROJECT_BINARY_DIR}
        ${compat_include_dir}
        ${gui_manager_SOURCE_DIR}/include
        ${vst3sdk_SOURCE_DIR}
        ${vst3sdk_SOURCE_DIR}/public.sdk
        ${vst3sdk_SOURCE_DIR}/vstgui4
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(gui_test_vst3
    PRIVATE
        DEVELOPMENT=1
        RELEASE=0
        CV_GUI_TEST_BACKEND="${CV_GUI_TEST_BACKEND}"
)

message(STATUS "GUIManager requested backend: ${CV_GUI_TEST_BACKEND}")
message(STATUS "GUIManager default selection: ImGuiBackend when requested/Auto; VSTGUIBackend currently probes and falls back unless full VSTGUI runtime is linked; NativeHostBackend returns nullptr for host editor.")

if(UNIX AND NOT APPLE)
    target_link_libraries(gui_test_vst3 PRIVATE dl pthread)
    set_target_properties(gui_test_vst3 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "gui_test_vst3"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/VST3/gui_test_vst3.vst3/Contents/x86_64-linux")
endif()
