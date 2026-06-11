cmake_minimum_required(VERSION 3.14.0)

project(cv_gm_instrument_lite_vst3
    VERSION 0.1.0.0
    DESCRIPTION "CV_DSP General MIDI SoundFont VST 3 Instrument"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(CV_GM_INSTRUMENT_LITE_ENABLE_CV_GUI "Use the CV_GUI editor when available" ON)

set(vst3sdk_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../backends/vst3sdk" CACHE PATH "Path to the bundled VST3 SDK")
if(NOT EXISTS "${vst3sdk_SOURCE_DIR}/pluginterfaces/base/funknown.h")
    message(FATAL_ERROR "Bundled VST3 SDK not found at ${vst3sdk_SOURCE_DIR}")
endif()

set(tinysoundfont_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../backends/TinySoundFont" CACHE PATH "Path to TinySoundFont")
if(EXISTS "${tinysoundfont_SOURCE_DIR}/tsf.h")
    set(CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT ON)
    message(STATUS "cv_gm_instrument_lite_vst3: TinySoundFont found at ${tinysoundfont_SOURCE_DIR}")
else()
    set(CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT OFF)
    message(WARNING "cv_gm_instrument_lite_vst3: TinySoundFont not found at ${tinysoundfont_SOURCE_DIR}; Prompt 10 documentation/tests closure runs in fallback mode when TinySoundFont is unavailable")
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
    ${vst3sdk_SOURCE_DIR}/public.sdk/common/commoniids.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/main/moduleinit.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/main/pluginfactory.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/common/pluginview.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/vst/vstinitiids.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/vst/vstaudioeffect.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/vst/vstbus.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/vst/vstcomponent.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/vst/vstcomponentbase.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/vst/vsteditcontroller.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/vst/vstparameters.cpp
)

if(WIN32)
    list(APPEND vst3_sdk_sources ${vst3sdk_SOURCE_DIR}/public.sdk/main/dllmain.cpp)
elseif(APPLE)
    list(APPEND vst3_sdk_sources ${vst3sdk_SOURCE_DIR}/public.sdk/main/macmain.cpp)
else()
    list(APPEND vst3_sdk_sources ${vst3sdk_SOURCE_DIR}/public.sdk/main/linuxmain.cpp)
endif()

if(CV_GM_INSTRUMENT_LITE_ENABLE_CV_GUI)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../CV_GUI ${PROJECT_BINARY_DIR}/CV_GUI)
endif()

add_library(cv_gm_instrument_lite_vst3 MODULE
    source/version.h
    source/TPcids.h
    source/TPprocessor.h
    source/SoundFontFolderScanner.h
    source/SoundFontEngine.h
    source/SoundFontEngine.cpp
    source/TPprocessor.cpp
    source/TPcontroller.h
    source/TPcontroller.cpp
    source/TPentry.cpp
    ${vst3_sdk_sources}
)

target_include_directories(cv_gm_instrument_lite_vst3
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../..
        ${PROJECT_BINARY_DIR}
        ${compat_include_dir}
        ${vst3sdk_SOURCE_DIR}
        ${vst3sdk_SOURCE_DIR}/public.sdk
)

if(CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT)
    target_include_directories(cv_gm_instrument_lite_vst3 PRIVATE ${tinysoundfont_SOURCE_DIR})
endif()

if(CV_GM_INSTRUMENT_LITE_ENABLE_CV_GUI)
    target_link_libraries(cv_gm_instrument_lite_vst3 PRIVATE CV_GUI::cv_gui)
endif()

if(CMAKE_DL_LIBS)
    target_link_libraries(cv_gm_instrument_lite_vst3 PRIVATE ${CMAKE_DL_LIBS})
endif()

target_compile_definitions(cv_gm_instrument_lite_vst3
    PRIVATE
        DEVELOPMENT=1
        RELEASE=0
        CV_GM_INSTRUMENT_LITE_ENABLE_CV_GUI=$<BOOL:${CV_GM_INSTRUMENT_LITE_ENABLE_CV_GUI}>
        CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT=$<BOOL:${CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT}>
)

if(WIN32)
    set_target_properties(cv_gm_instrument_lite_vst3 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "CV_GM_Instrument_Lite"
        SUFFIX ".vst3"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/out/CV_GM_Instrument_Lite"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/out/CV_GM_Instrument_Lite")
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(cv_gm_instrument_lite_vst3 PRIVATE dl pthread)
    set_target_properties(cv_gm_instrument_lite_vst3 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "CV_GM_Instrument_Lite"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/VST3/CV_GM_Instrument_Lite.vst3/Contents/x86_64-linux")
endif()

message(STATUS "cv_gm_instrument_lite_vst3: Prompt 10 documentation/tests closure configured")
message(STATUS "cv_gm_instrument_lite_vst3: expected sibling SoundFont folder is CV_GM_Instrument_Lite_SoundFonts")

add_executable(cv_gm_instrument_lite_foundation_smoke
    cv_gm_instrument_lite_foundation_smoke.cpp
    source/SoundFontEngine.cpp
    ${vst3sdk_SOURCE_DIR}/pluginterfaces/base/funknown.cpp
)

target_include_directories(cv_gm_instrument_lite_foundation_smoke
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../..
        ${vst3sdk_SOURCE_DIR}
)

if(CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT)
    target_include_directories(cv_gm_instrument_lite_foundation_smoke PRIVATE ${tinysoundfont_SOURCE_DIR})
endif()

target_compile_definitions(cv_gm_instrument_lite_foundation_smoke
    PRIVATE
        CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT=$<BOOL:${CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT}>
)
