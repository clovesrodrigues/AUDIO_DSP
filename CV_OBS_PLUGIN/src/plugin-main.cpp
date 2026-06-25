// plugin-main.cpp — Ponto de entrada do módulo OBS para o host VST3 AUDIO_DSP.
// Documentação em PT-BR conforme preferências do projeto.

#include <filesystem>
#include <obs-module.h>

#include "CV_OBS_PLUGIN/AudioDspVst3Filter.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-audio-dsp-vst3", "pt-BR")

namespace {
obs_source_info g_audioDspVst3FilterInfo =
    cv_obs_plugin::makeAudioDspVst3FilterInfo();
} // namespace

MODULE_EXPORT const char *obs_module_description(void) {
    return "AUDIO_DSP VST3 — host nativo de plugins VST3 como filtros de áudio do OBS.";
}

bool obs_module_load(void) {
    obs_register_source(&g_audioDspVst3FilterInfo);
    blog(LOG_INFO, "AUDIO_DSP VST3: módulo carregado (v%s)",
         OBS_AUDIO_DSP_VST3_PLUGIN_VERSION);
    return true;
}

// Executado após todos os módulos serem carregados.
// Não executa varredura automática: plugins VST3 de terceiros podem travar,
// demorar ou abrir dependências durante o boot do OBS. O usuário inicia a
// varredura explicitamente pelo botão "Scan VST3 Plugins" nas propriedades.
void obs_module_post_load(void) {
    bool cacheExists = false;
    if (char *cachePath = obs_module_config_path("obs-vst3/cache.json")) {
        std::error_code ec;
        cacheExists = std::filesystem::exists(cachePath, ec);
        bfree(cachePath);
    }

    if (cacheExists) {
        blog(LOG_INFO,
             "AUDIO_DSP VST3: cache VST3 encontrado — use 'Scan VST3 Plugins' para atualizar");
    } else {
        blog(LOG_INFO,
             "AUDIO_DSP VST3: cache não encontrado — use 'Scan VST3 Plugins' para iniciar a varredura");
    }
}
