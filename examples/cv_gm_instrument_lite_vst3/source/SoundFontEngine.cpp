//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "SoundFontEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
#define TSF_IMPLEMENTATION
#include "tsf.h"
#endif

namespace CV {

SoundFontEngine::~SoundFontEngine ()
{
    unload ();
}

bool SoundFontEngine::isBackendAvailable () noexcept
{
#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    return true;
#else
    return false;
#endif
}

void SoundFontEngine::prepare (double sampleRate) noexcept
{
    sampleRate_ = std::max (sampleRate, 1.0);
#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    if (soundFont_)
        tsf_set_output (static_cast<tsf*> (soundFont_), TSF_STEREO_INTERLEAVED, static_cast<int> (sampleRate_), 0.0f);
#else
    (void)sampleRate_;
#endif
}

void SoundFontEngine::reset () noexcept
{
    allNotesOff ();
}

bool SoundFontEngine::loadSoundFont (const std::filesystem::path& path)
{
#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    unload ();

    tsf* loaded = tsf_load_filename (path.string().c_str());
    if (!loaded)
        return false;

    soundFont_ = loaded;
    loadedPath_ = path;
    selectedPresetIndex_ = 0;
    tsf_set_output (loaded, TSF_STEREO_INTERLEAVED, static_cast<int> (sampleRate_), 0.0f);
    rebuildPresetList ();
    applySelectedPresetToChannel ();
    setPitchBend (pitchBend_);
    return true;
#else
    (void)path;
    unload ();
    return false;
#endif
}

void SoundFontEngine::unload () noexcept
{
#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    if (soundFont_)
    {
        tsf_close (static_cast<tsf*> (soundFont_));
        soundFont_ = nullptr;
    }
#endif

    loadedPath_.clear();
    presets_.clear();
    selectedPresetIndex_ = 0;
    selectedPresetListIndex_ = 0;
    pitchBend_ = 0.0f;
}

bool SoundFontEngine::isLoaded () const noexcept
{
    return soundFont_ != nullptr;
}

std::string SoundFontEngine::formatPresetDisplayName (int bank, int program, const std::string& name)
{
    char prefix[32] {};
    std::snprintf (prefix, sizeof (prefix), "%03d:%03d ", std::clamp (bank, 0, 999), std::clamp (program, 0, 999));
    return std::string (prefix) + name;
}

bool SoundFontEngine::selectPresetByListIndex (std::size_t listIndex) noexcept
{
    if (!isLoaded() || listIndex >= presets_.size())
        return false;

    allNotesOff ();
    selectedPresetListIndex_ = listIndex;
    selectedPresetIndex_ = presets_[listIndex].presetIndex;
    applySelectedPresetToChannel ();
    return true;
}

bool SoundFontEngine::selectPresetByIndex (int presetIndex) noexcept
{
    if (!isLoaded() || presetIndex < 0)
        return false;

    const auto found = std::find_if (presets_.begin(), presets_.end(), [presetIndex] (const SoundFontPresetInfo& preset) {
        return preset.presetIndex == presetIndex;
    });

    if (found == presets_.end())
        return false;

    allNotesOff ();
    selectedPresetListIndex_ = static_cast<std::size_t> (std::distance (presets_.begin(), found));
    selectedPresetIndex_ = presetIndex;
    applySelectedPresetToChannel ();
    return true;
}

void SoundFontEngine::noteOn (std::uint8_t note, float velocity) noexcept
{
#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    if (!soundFont_)
        return;

    tsf_channel_note_on (
        static_cast<tsf*> (soundFont_),
        kMainChannel,
        static_cast<int> (std::min<std::uint8_t> (note, 127)),
        std::clamp (velocity, 0.0f, 1.0f));
#else
    (void)note;
    (void)velocity;
#endif
}

void SoundFontEngine::noteOff (std::uint8_t note) noexcept
{
#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    if (!soundFont_)
        return;

    tsf_channel_note_off (
        static_cast<tsf*> (soundFont_),
        kMainChannel,
        static_cast<int> (std::min<std::uint8_t> (note, 127)));
#else
    (void)note;
#endif
}


void SoundFontEngine::setMidiController (std::uint8_t controller, std::uint8_t value) noexcept
{
#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    if (soundFont_)
        tsf_channel_midi_control (static_cast<tsf*> (soundFont_), kMainChannel, static_cast<int> (controller), static_cast<int> (value));
#else
    (void)controller;
    (void)value;
#endif
}

void SoundFontEngine::setPitchBend (float normalizedBend) noexcept
{
    pitchBend_ = std::clamp (normalizedBend, -1.0f, 1.0f);

#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    if (!soundFont_)
        return;

    const int wheel = static_cast<int> (std::lround ((pitchBend_ + 1.0f) * 8191.5f));
    tsf_channel_set_pitchwheel (static_cast<tsf*> (soundFont_), kMainChannel, std::clamp (wheel, 0, 16383));
#endif
}

void SoundFontEngine::allNotesOff () noexcept
{
#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    if (soundFont_)
    {
        tsf_channel_note_off_all (static_cast<tsf*> (soundFont_), kMainChannel);
        tsf_channel_sounds_off_all (static_cast<tsf*> (soundFont_), kMainChannel);
    }
#endif
}

void SoundFontEngine::renderInterleavedStereo (float* output, int sampleFrames, bool mix) noexcept
{
    if (!output || sampleFrames <= 0)
        return;

#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    if (!soundFont_)
        return;

    tsf_render_float (static_cast<tsf*> (soundFont_), output, sampleFrames, mix ? 1 : 0);
#else
    (void)mix;
#endif
}

void SoundFontEngine::rebuildPresetList ()
{
    presets_.clear();
    selectedPresetIndex_ = 0;
    selectedPresetListIndex_ = 0;

#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    if (!soundFont_)
        return;

    tsf* font = static_cast<tsf*> (soundFont_);
    const int presetCount = tsf_get_presetcount (font);
    presets_.reserve (static_cast<std::size_t> (std::max (presetCount, 0)));

    for (int index = 0; index < presetCount; ++index)
    {
        const char* name = tsf_get_presetname (font, index);
        const int bank = 0;
        const int program = index;
        const std::string presetName = name ? name : "Unnamed preset";
        presets_.push_back ({
            index,
            bank,
            program,
            presetName,
            formatPresetDisplayName (bank, program, presetName)});
    }

    std::sort (presets_.begin(), presets_.end(), [] (const SoundFontPresetInfo& left, const SoundFontPresetInfo& right) {
        if (left.bank != right.bank)
            return left.bank < right.bank;
        if (left.program != right.program)
            return left.program < right.program;
        return left.name < right.name;
    });

    if (!presets_.empty())
        selectedPresetIndex_ = presets_.front().presetIndex;
#endif
}

void SoundFontEngine::applySelectedPresetToChannel () noexcept
{
#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    if (soundFont_)
        tsf_channel_set_presetindex (static_cast<tsf*> (soundFont_), kMainChannel, selectedPresetIndex_);
#endif
}

} // namespace CV
