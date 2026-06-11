cmake_minimum_required(VERSION 3.14.0)

project(cv_bass_finger_lite_vst3
    VERSION 0.1.0.0
    DESCRIPTION "CV_DSP Bass Finger Lite VST 3 Instrument"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(vst3sdk_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../backends/vst3sdk" CACHE PATH "Path to the bundled VST3 SDK")
if(NOT EXISTS "${vst3sdk_SOURCE_DIR}/pluginterfaces/base/funknown.h")
    message(FATAL_ERROR "Bundled VST3 SDK not found at ${vst3sdk_SOURCE_DIR}")
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

add_library(cv_bass_finger_lite_vst3 MODULE
    source/version.h
    source/TPcids.h
    source/TPprocessor.h
    source/TPprocessor.cpp
    source/TPcontroller.h
    source/TPcontroller.cpp
    source/TPentry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../CV_DSP/Manager/MidiEvent.hpp
    ${CMAKE_CURRENT_LIST_DIR}/../../CV_DSP/Manager/MonoNoteTracker.hpp
    ${CMAKE_CURRENT_LIST_DIR}/../../CV_DSP/Synthesis/Bass/BassFingerVoice.hpp
    ${CMAKE_CURRENT_LIST_DIR}/../../CV_DSP/Reverb/RoomReverb.hpp
    ${vst3_sdk_sources}
)

target_include_directories(cv_bass_finger_lite_vst3
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../..
        ${PROJECT_BINARY_DIR}
        ${compat_include_dir}
        ${vst3sdk_SOURCE_DIR}
        ${vst3sdk_SOURCE_DIR}/public.sdk
)

target_compile_definitions(cv_bass_finger_lite_vst3
    PRIVATE
        DEVELOPMENT=1
        RELEASE=0
)

if(WIN32)
    set_target_properties(cv_bass_finger_lite_vst3 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "cv_bass_finger_lite_vst3"
        SUFFIX ".vst3"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/out/cv_bass_finger_lite_vst3"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/out/cv_bass_finger_lite_vst3")
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(cv_bass_finger_lite_vst3 PRIVATE dl pthread)
    set_target_properties(cv_bass_finger_lite_vst3 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "cv_bass_finger_lite_vst3"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/VST3/cv_bass_finger_lite_vst3.vst3/Contents/x86_64-linux")
endif()

message(STATUS "cv_bass_finger_lite_vst3: instrument target configured")

add_executable(cv_bass_finger_lite_midi_smoke
    cv_bass_midi_tracker_smoke.cpp
)

target_include_directories(cv_bass_finger_lite_midi_smoke
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../..
)

add_executable(cv_bass_finger_lite_voice_smoke
    cv_bass_voice_smoke.cpp
)

target_include_directories(cv_bass_finger_lite_voice_smoke
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../..
)
