cmake_minimum_required(VERSION 3.16)

project(obs-audio-dsp-vst3
    VERSION 0.1.0
    DESCRIPTION "AUDIO_DSP native OBS audio filter with VST3 hosting support"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(LIBOBS_INCLUDE_DIR "" CACHE PATH "Path containing obs-module.h, normally the OBS/libobs include directory")
set(LIBOBS_LIBRARY "" CACHE FILEPATH "Path to the OBS/libobs library")

if(NOT LIBOBS_INCLUDE_DIR)
    find_path(LIBOBS_INCLUDE_DIR
        NAMES obs-module.h
        PATH_SUFFIXES libobs include obs
        DOC "Path containing obs-module.h")
endif()

if(NOT LIBOBS_LIBRARY)
    find_library(LIBOBS_LIBRARY
        NAMES obs libobs
        DOC "Path to OBS/libobs library")
endif()

if(NOT LIBOBS_INCLUDE_DIR OR NOT EXISTS "${LIBOBS_INCLUDE_DIR}/obs-module.h")
    message(FATAL_ERROR
        "obs-module.h was not found. Configure with -DLIBOBS_INCLUDE_DIR=/path/to/obs/libobs or the matching OBS SDK include path.")
endif()

if(NOT LIBOBS_LIBRARY)
    message(FATAL_ERROR
        "OBS/libobs library was not found. Configure with -DLIBOBS_LIBRARY=/path/to/obs/libobs library.")
endif()

add_library(obs-audio-dsp-vst3 MODULE
    src/audio-dsp-vst3-filter.cpp
    src/plugin-main.cpp
)

target_include_directories(obs-audio-dsp-vst3
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../backends/vst3sdk
        ${LIBOBS_INCLUDE_DIR}
)

target_link_libraries(obs-audio-dsp-vst3
    PRIVATE
        ${LIBOBS_LIBRARY}
        ${CMAKE_DL_LIBS}
)

target_compile_definitions(obs-audio-dsp-vst3
    PRIVATE
        OBS_AUDIO_DSP_VST3_PLUGIN_VERSION="${PROJECT_VERSION}"
)

set_target_properties(obs-audio-dsp-vst3 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "obs-audio-dsp-vst3"
)

if(WIN32)
    set_target_properties(obs-audio-dsp-vst3 PROPERTIES SUFFIX ".dll")
endif()
