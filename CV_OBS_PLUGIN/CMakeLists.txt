cmake_minimum_required(VERSION 3.16)

project(obs-audio-dsp-vst3
    VERSION 0.1.0
    DESCRIPTION "AUDIO_DSP native OBS audio filter with full VST3 hosting (scanner, parameters, native GUI)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ─── RESOLUÇÃO DE CAMINHOS ABSOLUTOS ──────────────────────────────────────────
# Obrigatório: o GCC MinGW falha ao resolver caminhos com ".." passados via -I.
# get_filename_component com ABSOLUTE expande o caminho antes de injetar no compilador.
get_filename_component(WORKSPACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

set(PLUGIN_INCLUDE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(CV_DSP_INCLUDE_DIR  "${WORKSPACE_DIR}")
set(OBS_DIR             "${WORKSPACE_DIR}/backends/obs-studio/libobs")
set(IMGUI_DIR           "${WORKSPACE_DIR}/backends/imgui")
set(VST3_SDK_DIR        "${WORKSPACE_DIR}/backends/vst3sdk")

# ─── TRAVAS DE SEGURANÇA (aborta com mensagem clara antes de qualquer compilação) ─
if(NOT EXISTS "${OBS_DIR}/obs-module.h")
    message(FATAL_ERROR
        "[CV_OBS_PLUGIN] obs-module.h não encontrado em: ${OBS_DIR}\n"
        "Verifique se backends/obs-studio/libobs/ existe na raiz do workspace.")
endif()
if(NOT EXISTS "${VST3_SDK_DIR}/pluginterfaces/vst/ivsteditcontroller.h")
    message(FATAL_ERROR
        "[CV_OBS_PLUGIN] ivsteditcontroller.h não encontrado em: ${VST3_SDK_DIR}\n"
        "Verifique se backends/vst3sdk/ existe na raiz do workspace.")
endif()

# ─── LINKAGEM DO OBS STUDIO (SDK bundled, sem package manager) ───────────────
find_library(LIBOBS_LIBRARY NAMES obs libobs PATHS "${OBS_DIR}" NO_DEFAULT_PATH)
if(NOT LIBOBS_LIBRARY)
    link_directories("${OBS_DIR}")
    set(LIBOBS_LIBRARY "-lobs")
endif()

# ─── SMTG_DEFINE_IID ─────────────────────────────────────────────────────────
# Necessário para que o VST3 SDK instancie as constantes IID globais no MinGW/MSVC.
# Sem isso: "undefined reference to Steinberg::IPluginFactory2::iid"
add_definitions(-DSMTG_DEFINE_IID)

# ─── FONTES DO PLUGIN ─────────────────────────────────────────────────────────
add_library(obs-audio-dsp-vst3 MODULE
    src/audio-dsp-vst3-filter.cpp
    src/plugin-main.cpp
    src/Vst3Scanner.cpp
    src/Vst3ImGuiFallback.cpp

    # ImGui core
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"

    # VST3 SDK — FUID/IID runtime (somente como host; public.sdk não é necessário)
    "${VST3_SDK_DIR}/pluginterfaces/base/funknown.cpp"
)

# ─── INCLUDES (todos absolutos para evitar bug relativo do GCC MinGW) ─────────
target_include_directories(obs-audio-dsp-vst3
    PRIVATE
        "${PLUGIN_INCLUDE_DIR}"
        "${CV_DSP_INCLUDE_DIR}"
        "${VST3_SDK_DIR}"
        "${OBS_DIR}"
        "${IMGUI_DIR}"
        "${IMGUI_DIR}/backends"
)

# ─── MACROS GLOBAIS ───────────────────────────────────────────────────────────
target_compile_definitions(obs-audio-dsp-vst3
    PRIVATE
        OBS_AUDIO_DSP_VST3_PLUGIN_VERSION="${PROJECT_VERSION}"
        # NOMINMAX definido aqui globalmente — NÃO repetir nos .cpp
        # (MinGW 15.2.0 já define em os_defines.h; redefinição manual causa warning)
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        # Evita conflito entre SIMDE (bundled no OBS) e intrínsecos nativos do GCC 15
        SIMDE_NO_NATIVE_P_FUNCTIONS=1
        SIMDE_X86_SSE_FORCE_NO_NATIVE=1
)

# ─── LINKS BASE ───────────────────────────────────────────────────────────────
target_link_libraries(obs-audio-dsp-vst3
    PRIVATE
        ${LIBOBS_LIBRARY}
        ${CMAKE_DL_LIBS}
)

# ─── PLATAFORMA: Windows ──────────────────────────────────────────────────────
if(WIN32)
    # Keep OBS module entry points in the PE/COFF export table for MinGW/MSVC.
    target_sources(obs-audio-dsp-vst3 PRIVATE
        "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/obs-audio-dsp-vst3.def"
    )
    target_link_libraries(obs-audio-dsp-vst3 PRIVATE
        opengl32
        gdi32
        # dwmapi: exigido pelo imgui_impl_win32 (DwmIsCompositionEnabled)
        # Sem isso: "undefined reference to DwmIsCompositionEnabled"
        dwmapi
    )
    set_target_properties(obs-audio-dsp-vst3 PROPERTIES SUFFIX ".dll")

# ─── PLATAFORMA: Linux ────────────────────────────────────────────────────────
elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    target_link_libraries(obs-audio-dsp-vst3 PRIVATE
        X11::X11
        OpenGL::GL
    )
    # GLX está dentro de libGL na maioria das distros; target separado quando disponível
    if(TARGET OpenGL::GLX)
        target_link_libraries(obs-audio-dsp-vst3 PRIVATE OpenGL::GLX)
    endif()
endif()

# ─── PROPRIEDADES DO ALVO ─────────────────────────────────────────────────────
set_target_properties(obs-audio-dsp-vst3 PROPERTIES
    PREFIX      ""
    OUTPUT_NAME "obs-audio-dsp-vst3"
)
