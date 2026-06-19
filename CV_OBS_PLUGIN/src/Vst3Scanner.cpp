// Vst3Scanner.cpp — Varredura automática de diretórios VST3 e geração do cache.json
// Documentação e comentários em PT-BR conforme preferências do projeto.

#include "CV_OBS_PLUGIN/Vst3Scanner.hpp"
#include "CV_OBS_PLUGIN/Vst3StorageModel.hpp"

#include <obs-module.h>

#include "pluginterfaces/base/ipluginbase.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace cv_obs_plugin {
namespace {

// ─── Carregador de biblioteca para varredura ──────────────────────────────────

struct ScanLibrary {
#if defined(_WIN32)
    using Handle = HMODULE;
#else
    using Handle = void*;
#endif

    ScanLibrary() = default;

    explicit ScanLibrary(const std::filesystem::path& path) {
#if defined(_WIN32)
        handle_ = ::LoadLibraryW(path.wstring().c_str());
        if (handle_) {
            using InitProc = bool (PLUGIN_API*)();
            if (auto* init = reinterpret_cast<InitProc>(::GetProcAddress(handle_, "InitDll")))
                if (!init()) close();
        }
#else
        handle_ = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle_) {
            using ModEntryProc = bool (PLUGIN_API*)(void*);
            if (auto* mod = reinterpret_cast<ModEntryProc>(::dlsym(handle_, "ModuleEntry")))
                if (!mod(handle_)) close();
        }
#endif
    }

    ~ScanLibrary() { close(); }
    ScanLibrary(const ScanLibrary&)            = delete;
    ScanLibrary& operator=(const ScanLibrary&) = delete;

    [[nodiscard]] bool isOpen() const noexcept { return handle_ != nullptr; }

    template<typename FuncPtr>
    [[nodiscard]] FuncPtr loadSymbol(const char* name) const {
        if (!handle_) return nullptr;
#if defined(_WIN32)
        return reinterpret_cast<FuncPtr>(::GetProcAddress(handle_, name));
#else
        return reinterpret_cast<FuncPtr>(::dlsym(handle_, name));
#endif
    }

private:
    void close() noexcept {
        if (!handle_) return;
#if defined(_WIN32)
        using ExitProc = bool (PLUGIN_API*)();
        if (auto* ex = reinterpret_cast<ExitProc>(::GetProcAddress(handle_, "ExitDll")))
            ex();
        ::FreeLibrary(handle_);
#else
        using ModExitProc = bool (PLUGIN_API*)();
        if (auto* ex = reinterpret_cast<ModExitProc>(::dlsym(handle_, "ModuleExit")))
            ex();
        ::dlclose(handle_);
#endif
        handle_ = nullptr;
    }

    Handle handle_ = nullptr;
};

// ─── Resolução do binário dentro de um bundle .vst3 ──────────────────────────

[[nodiscard]] std::filesystem::path resolveBinaryInBundle(
        const std::filesystem::path& bundlePath) {

    std::error_code ec;
    if (std::filesystem::is_regular_file(bundlePath, ec) && !ec)
        return bundlePath;

    const std::string stem = bundlePath.stem().string();

#if defined(_WIN32)
    const std::array<std::filesystem::path, 3> candidates = {
        bundlePath / "Contents" / "x86_64-win" / (stem + ".dll"),
        bundlePath / "Contents" / "x86-win"    / (stem + ".dll"),
        bundlePath / (stem + ".dll"),
    };
#elif defined(__APPLE__)
    const std::array<std::filesystem::path, 2> candidates = {
        bundlePath / "Contents" / "MacOS" / stem,
        bundlePath / "Contents" / "MacOS" / (stem + ".dylib"),
    };
#else
    const std::array<std::filesystem::path, 4> candidates = {
        bundlePath / "Contents" / "x86_64-linux"  / (stem + ".so"),
        bundlePath / "Contents" / "aarch64-linux"  / (stem + ".so"),
        bundlePath / "Contents" / "amd64-linux"    / (stem + ".so"),
        bundlePath / (stem + ".so"),
    };
#endif

    for (const auto& c : candidates) {
        std::error_code cec;
        if (std::filesystem::is_regular_file(c, cec) && !cec)
            return c;
    }
    return {};
}

// ─── TUID → string hexadecimal de 32 chars ────────────────────────────────────

[[nodiscard]] std::string tuidToHexString(const Steinberg::TUID id) {
    char buf[33] = {};
    for (int i = 0; i < 16; ++i)
        std::snprintf(buf + i * 2, 3, "%02x", static_cast<unsigned char>(id[i]));
    return std::string(buf);
}

// ─── Escape de string para JSON ───────────────────────────────────────────────

[[nodiscard]] std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:   out += static_cast<char>(c); break;
        }
    }
    return out;
}

// ─── Diretórios padrão de varredura por plataforma ────────────────────────────

[[nodiscard]] std::vector<std::filesystem::path> standardScanDirectories() {
    std::vector<std::filesystem::path> dirs;

#if defined(_WIN32)
    if (const char* pf64 = std::getenv("CommonProgramFiles"))
        dirs.emplace_back(std::filesystem::path(pf64) / "VST3");
    if (const char* local = std::getenv("LOCALAPPDATA"))
        dirs.emplace_back(std::filesystem::path(local) / "Programs" / "Common" / "VST3");
    if (const char* pf = std::getenv("ProgramFiles"))
        dirs.emplace_back(std::filesystem::path(pf) / "Common Files" / "VST3");

#elif defined(__APPLE__)
    dirs.emplace_back("/Library/Audio/Plug-Ins/VST3");
    if (const char* home = std::getenv("HOME"))
        dirs.emplace_back(std::filesystem::path(home) / "Library" / "Audio" / "Plug-Ins" / "VST3");

#else  // Linux
    dirs.emplace_back("/usr/lib/vst3");
    dirs.emplace_back("/usr/local/lib/vst3");
    dirs.emplace_back("/usr/lib/x86_64-linux-gnu/vst3");
    if (const char* home = std::getenv("HOME"))
        dirs.emplace_back(std::filesystem::path(home) / ".vst3");
#endif

    return dirs;
}

// ─── Coleta recursiva de bundles .vst3 ───────────────────────────────────────

void collectVst3Bundles(const std::filesystem::path& dir,
                         std::vector<std::filesystem::path>& out) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec) || ec)
        return;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (ec) break;
        if (entry.path().extension() == ".vst3")
            out.push_back(entry.path());
    }
}

// ─── Varredura de um único bundle .vst3 ──────────────────────────────────────

[[nodiscard]] std::vector<Vst3PluginCacheEntry> scanSingleBundle(
        const std::filesystem::path& bundlePath) {

    const std::filesystem::path binaryPath = resolveBinaryInBundle(bundlePath);
    if (binaryPath.empty()) {
        blog(LOG_DEBUG, "AUDIO_DSP VST3 scanner: binário não encontrado em %s",
             bundlePath.string().c_str());
        return {};
    }

    ScanLibrary lib(binaryPath);
    if (!lib.isOpen()) {
        blog(LOG_DEBUG, "AUDIO_DSP VST3 scanner: falha ao abrir biblioteca %s",
             binaryPath.string().c_str());
        return {};
    }

    using GetFactoryProc = Steinberg::IPluginFactory* (PLUGIN_API*)();
    auto* getFactory = lib.loadSymbol<GetFactoryProc>("GetPluginFactory");
    if (!getFactory)
        return {};

    Steinberg::IPluginFactory* factory = getFactory();
    if (!factory)
        return {};

    // Tenta IPluginFactory2 para obter vendor e versão
    Steinberg::IPluginFactory2* factory2 = nullptr;
    factory->queryInterface(Steinberg::IPluginFactory2::iid,
                            reinterpret_cast<void**>(&factory2));

    // Informações do fabricante a nível de factory
    Steinberg::PFactoryInfo factoryInfo = {};
    factory->getFactoryInfo(&factoryInfo);

    const std::uint64_t now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::vector<Vst3PluginCacheEntry> entries;
    const Steinberg::int32 classCount = factory->countClasses();

    for (Steinberg::int32 i = 0; i < classCount; ++i) {
        Steinberg::PClassInfo info = {};
        if (factory->getClassInfo(i, &info) != Steinberg::kResultOk)
            continue;

        // Apenas componentes de áudio
        if (std::string_view(info.category) != "Audio Module Class")
            continue;

        Vst3PluginCacheEntry entry;
        entry.path     = bundlePath.string();
        entry.name     = std::string(info.name);
        entry.classId  = tuidToHexString(info.cid);
        entry.category = std::string(info.category);
        entry.status   = Vst3ScanStatus::MetadataRead;
        entry.vendor   = std::string(factoryInfo.vendor);
        entry.scannedAtUnixSeconds = now;

        // IPluginFactory2 fornece vendor e versão por componente (mais preciso)
        if (factory2) {
            Steinberg::PClassInfo2 info2 = {};
            if (factory2->getClassInfo2(i, &info2) == Steinberg::kResultOk) {
                if (info2.vendor[0])  entry.vendor  = std::string(info2.vendor);
                if (info2.version[0]) entry.version = std::string(info2.version);
            }
        }

        entries.push_back(std::move(entry));
    }

    if (factory2) factory2->release();
    factory->release();

    return entries;
}

// ─── Gravação do cache JSON ───────────────────────────────────────────────────

bool writeVst3Cache(const std::filesystem::path& cachePath,
                    const std::vector<Vst3PluginCacheEntry>& plugins) {
    std::error_code ec;
    std::filesystem::create_directories(cachePath.parent_path(), ec);
    if (ec) {
        blog(LOG_WARNING, "AUDIO_DSP VST3 scanner: falha ao criar diretório de cache: %s",
             ec.message().c_str());
        return false;
    }

    std::ofstream f(cachePath, std::ios::out | std::ios::trunc);
    if (!f) {
        blog(LOG_WARNING, "AUDIO_DSP VST3 scanner: não foi possível abrir %s para escrita",
             cachePath.string().c_str());
        return false;
    }

    f << "{\n  \"schemaVersion\": " << vst3_storage::kCacheSchemaVersion
      << ",\n  \"plugins\": [";

    bool first = true;
    for (const auto& p : plugins) {
        if (!first) f << ',';
        first = false;
        f << "\n    {\n"
          << "      \"path\":                 \"" << jsonEscape(p.path)    << "\",\n"
          << "      \"name\":                 \"" << jsonEscape(p.name)    << "\",\n"
          << "      \"vendor\":               \"" << jsonEscape(p.vendor)  << "\",\n"
          << "      \"version\":              \"" << jsonEscape(p.version) << "\",\n"
          << "      \"classId\":              \"" << p.classId             << "\",\n"
          << "      \"category\":             \"" << jsonEscape(p.category)<< "\",\n"
          << "      \"status\":               \"MetadataRead\",\n"
          << "      \"lastModifiedUnixSeconds\": 0,\n"
          << "      \"scannedAtUnixSeconds\":    " << p.scannedAtUnixSeconds << "\n"
          << "    }";
    }

    f << "\n  ]\n}\n";
    return f.good();
}

} // namespace anônimo

// ─── API pública ──────────────────────────────────────────────────────────────

int scanAndCacheVst3Plugins() {
    blog(LOG_INFO, "AUDIO_DSP VST3: iniciando varredura de diretórios VST3...");

    auto dirs = standardScanDirectories();

    // Verifica também obs-vst3/plugins/ dentro da configuração do módulo OBS
    if (char* obsPluginsPath = obs_module_config_path("obs-vst3/plugins")) {
        dirs.emplace_back(std::filesystem::path(obsPluginsPath));
        bfree(obsPluginsPath);
    }

    std::vector<std::filesystem::path> bundles;
    for (const auto& dir : dirs) {
        blog(LOG_INFO, "AUDIO_DSP VST3: varrendo: %s", dir.string().c_str());
        collectVst3Bundles(dir, bundles);
    }

    blog(LOG_INFO, "AUDIO_DSP VST3: encontrados %zu bundle(s) .vst3", bundles.size());

    std::vector<Vst3PluginCacheEntry> allPlugins;
    for (const auto& bundle : bundles) {
        auto entries = scanSingleBundle(bundle);
        for (auto& e : entries)
            allPlugins.push_back(std::move(e));
    }

    // Grava o cache
    bool cacheWritten = false;
    if (char* cachePath = obs_module_config_path("obs-vst3/cache.json")) {
        cacheWritten = writeVst3Cache(std::filesystem::path(cachePath), allPlugins);
        bfree(cachePath);
    } else {
        blog(LOG_ERROR, "AUDIO_DSP VST3: obs_module_config_path retornou null");
        return -1;
    }

    if (cacheWritten) {
        blog(LOG_INFO, "AUDIO_DSP VST3: varredura concluída — %zu componente(s) de áudio em cache",
             allPlugins.size());
    } else {
        blog(LOG_WARNING, "AUDIO_DSP VST3: varredura concluída, mas falha ao gravar cache");
    }

    return static_cast<int>(allPlugins.size());
}

} // namespace cv_obs_plugin
