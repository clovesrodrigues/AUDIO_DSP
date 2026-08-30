cmake_minimum_required(VERSION 3.14.0)

project(graphic_eq_vst2
    VERSION 1.0.0.0
    DESCRIPTION "CV_DSP Graphic EQ VST 2 Plug-in"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(vst2sdk_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../backends/VST2_SDK_2.4" CACHE PATH "Path to the bundled VST2 SDK")
if(NOT EXISTS "${vst2sdk_SOURCE_DIR}/public.sdk/source/vst2.x/audioeffectx.h")
    message(FATAL_ERROR "Bundled VST2 SDK not found at ${vst2sdk_SOURCE_DIR}")
endif()

set(vst2_sdk_sources
    ${vst2sdk_SOURCE_DIR}/public.sdk/source/vst2.x/audioeffect.cpp
    ${vst2sdk_SOURCE_DIR}/public.sdk/source/vst2.x/audioeffectx.cpp
)

add_library(graphic_eq_vst2 MODULE
    source/GraphicEQVST2.hpp
    source/GraphicEQVST2.cpp
    source/GraphicEQVST2Entry.cpp
    ${vst2_sdk_sources}
)

target_include_directories(graphic_eq_vst2
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../..
)

target_include_directories(graphic_eq_vst2
    SYSTEM PRIVATE
        ${vst2sdk_SOURCE_DIR}
        ${vst2sdk_SOURCE_DIR}/public.sdk/source/vst2.x
)

target_compile_definitions(graphic_eq_vst2
    PRIVATE
        DEVELOPMENT=1
        RELEASE=0
)

if(MSVC)
    target_compile_options(graphic_eq_vst2 PRIVATE /W4)
else()
    target_compile_options(graphic_eq_vst2 PRIVATE -Wall -Wextra -Wpedantic)
    set_source_files_properties(${vst2_sdk_sources} PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter")
endif()

if(WIN32)
    set_target_properties(graphic_eq_vst2 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "graphic_eq_vst2"
        SUFFIX ".dll"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/out/vst2"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/out/vst2")
else()
    set_target_properties(graphic_eq_vst2 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "graphic_eq_vst2"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/out/vst2")
endif()
