cmake_minimum_required(VERSION 3.16)

project(obs-audio-dsp-vst3
    VERSION 0.1.0
    DESCRIPTION "AUDIO_DSP native OBS audio filter with full VST3 hosting (scanner, parameters, native GUI)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ─── OBS Studio ──────────────────────────────────────────────────────────────

set(LIBOBS_INCLUDE_DIR "" CACHE PATH
    "Path containing obs-module.h (e.g. /usr/include/obs or the OBS SDK include dir)")
set(LIBOBS_LIBRARY "" CACHE FILEPATH
    "Path to the OBS/libobs shared library (e.g. libobs.so or obs.lib)")

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
        "obs-module.h was not found. Set -DLIBOBS_INCLUDE_DIR=/path/to/obs/libobs")
endif()

if(NOT LIBOBS_LIBRARY)
    message(FATAL_ERROR
        "OBS/libobs library not found. Set -DLIBOBS_LIBRARY=/path/to/libobs")
endif()

# ─── ImGui (Dear ImGui — fallback parameter editor) ──────────────────────────

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../backends/imgui")

# ─── Fontes do plugin ─────────────────────────────────────────────────────────

add_library(obs-audio-dsp-vst3 MODULE
    src/audio-dsp-vst3-filter.cpp
    src/plugin-main.cpp
    src/Vst3Scanner.cpp
    src/Vst3ImGuiFallback.cpp
    # ImGui core
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    # ImGui backend OpenGL3 (independente de plataforma)
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# ─── Includes ────────────────────────────────────────────────────────────────

target_include_directories(obs-audio-dsp-vst3
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../backends/vst3sdk
        ${LIBOBS_INCLUDE_DIR}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

# ─── Links base ──────────────────────────────────────────────────────────────

target_link_libraries(obs-audio-dsp-vst3
    PRIVATE
        ${LIBOBS_LIBRARY}
        ${CMAKE_DL_LIBS}
)

# ─── Links e sources específicos de plataforma ───────────────────────────────

if(WIN32)
    # Windows: Win32 + WGL para o editor ImGui fallback
    target_sources(obs-audio-dsp-vst3 PRIVATE
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    )
    target_link_libraries(obs-audio-dsp-vst3 PRIVATE opengl32 gdi32)

elseif(UNIX AND NOT APPLE)
    # Linux: X11 + OpenGL (GLX) para janela nativa VST3 e editor ImGui fallback
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    target_link_libraries(obs-audio-dsp-vst3 PRIVATE
        X11::X11
        OpenGL::GL
    )
    # GLX está dentro de libGL na maioria das distros Linux
    if(TARGET OpenGL::GLX)
        target_link_libraries(obs-audio-dsp-vst3 PRIVATE OpenGL::GLX)
    endif()
endif()

# ─── Definições de compilação ─────────────────────────────────────────────────

target_compile_definitions(obs-audio-dsp-vst3
    PRIVATE
        OBS_AUDIO_DSP_VST3_PLUGIN_VERSION="${PROJECT_VERSION}"
)

# ─── Propriedades do alvo ─────────────────────────────────────────────────────

set_target_properties(obs-audio-dsp-vst3 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "obs-audio-dsp-vst3"
)

if(WIN32)
    set_target_properties(obs-audio-dsp-vst3 PROPERTIES SUFFIX ".dll")
endif()
