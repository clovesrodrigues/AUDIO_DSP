//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace CV {

struct SoundFontPresetInfo
{
    int presetIndex {-1};
    int bank {0};
    int program {0};
    std::string name {};
    std::string displayName {};
};

class SoundFontEngine
{
public:
    SoundFontEngine () = default;
    ~SoundFontEngine ();

    SoundFontEngine (const SoundFontEngine&) = delete;
    SoundFontEngine& operator= (const SoundFontEngine&) = delete;
    SoundFontEngine (SoundFontEngine&&) = delete;
    SoundFontEngine& operator= (SoundFontEngine&&) = delete;

    static bool isBackendAvailable () noexcept;

    void prepare (double sampleRate) noexcept;
    void reset () noexcept;
    bool loadSoundFont (const std::filesystem::path& path);
    void unload () noexcept;

    bool isLoaded () const noexcept;
    const std::filesystem::path& loadedPath () const noexcept { return loadedPath_; }
    const std::vector<SoundFontPresetInfo>& presets () const noexcept { return presets_; }

    static std::string formatPresetDisplayName (int bank, int program, const std::string& name);

    bool selectPresetByListIndex (std::size_t listIndex) noexcept;
    bool selectPresetByIndex (int presetIndex) noexcept;
    int selectedPresetIndex () const noexcept { return selectedPresetIndex_; }
    std::size_t selectedPresetListIndex () const noexcept { return selectedPresetListIndex_; }
    float pitchBend () const noexcept { return pitchBend_; }

    void noteOn (std::uint8_t note, float velocity) noexcept;
    void noteOff (std::uint8_t note) noexcept;
    void setMidiController (std::uint8_t controller, std::uint8_t value) noexcept;
    void setPitchBend (float normalizedBend) noexcept;
    void allNotesOff () noexcept;
    void renderInterleavedStereo (float* output, int sampleFrames, bool mix) noexcept;

private:
    static constexpr int kMainChannel = 0;

    void rebuildPresetList ();
    void applySelectedPresetToChannel () noexcept;

    void* soundFont_ {nullptr};
    std::filesystem::path loadedPath_ {};
    std::vector<SoundFontPresetInfo> presets_ {};
    double sampleRate_ {44100.0};
    int selectedPresetIndex_ {0};
    std::size_t selectedPresetListIndex_ {0};
    float pitchBend_ {0.0f};
};

} // namespace CV
