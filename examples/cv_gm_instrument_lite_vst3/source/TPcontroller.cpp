//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"
#include "SoundFontFolderScanner.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <dlfcn.h>
#endif

#if CV_GM_INSTRUMENT_LITE_ENABLE_CV_GUI
#include "CV_GUI/VST3ImGuiView.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kVolumeDefault = 1.0;
constexpr double kEqDefault = 0.0;
constexpr double kRoomDefault = 0.0;

Vst::ParamValue normalizeEq (double plainDb)
{
    return (plainDb + 12.0) / 24.0;
}

void copyAsciiToString128 (const std::string& text, Vst::String128 destination)
{
    std::fill_n (destination, 128, 0);
    const auto count = std::min<std::size_t> (text.size(), 127);
    for (std::size_t index = 0; index < count; ++index)
        destination[index] = static_cast<Vst::TChar> (static_cast<unsigned char> (text[index]));
}

void appendAsciiString (Vst::StringListParameter& parameter, const std::string& text)
{
    Vst::String128 value {};
    copyAsciiToString128 (text, value);
    parameter.appendString (value);
}

std::size_t normalizedToListIndex (Vst::ParamValue normalized, std::size_t itemCount) noexcept
{
    if (itemCount == 0)
        return 0;

    const auto maxIndex = static_cast<double> (itemCount - 1);
    const auto index = static_cast<std::size_t> (std::lround (std::clamp (normalized, 0.0, 1.0) * maxIndex));
    return std::min (index, itemCount - 1);
}

Vst::ParamValue listIndexToNormalized (std::size_t index, std::size_t itemCount) noexcept
{
    if (itemCount <= 1)
        return 0.0;

    return static_cast<double> (std::min (index, itemCount - 1)) / static_cast<double> (itemCount - 1);
}

std::filesystem::path resolvePluginModulePath ()
{
#if defined(_WIN32)
    HMODULE moduleHandle = nullptr;
    const auto flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (GetModuleHandleExA (flags, reinterpret_cast<LPCSTR> (&resolvePluginModulePath), &moduleHandle) && moduleHandle)
    {
        char modulePath[MAX_PATH] {};
        const DWORD length = GetModuleFileNameA (moduleHandle, modulePath, MAX_PATH);
        if (length > 0)
            return std::filesystem::path (modulePath);
    }
#elif defined(__APPLE__) || defined(__linux__)
    Dl_info info {};
    if (dladdr (reinterpret_cast<void*> (&resolvePluginModulePath), &info) && info.dli_fname)
        return std::filesystem::path (info.dli_fname);
#endif

    return std::filesystem::current_path() / "CV_GM_Instrument_Lite.vst3";
}

std::filesystem::path resolveControllerSoundFontFolder ()
{
    return SoundFontFolderScanner::resolveSiblingSoundFontsFolder (resolvePluginModulePath ());
}

Vst::StringListParameter* getStringListParameter (Vst::EditController& controller, Vst::ParamID paramID)
{
    return static_cast<Vst::StringListParameter*> (controller.getParameterObject (paramID));
}

void replaceStringList (Vst::StringListParameter& parameter, const std::vector<std::string>& labels)
{
    parameter.clear ();
    if (labels.empty())
    {
        appendAsciiString (parameter, "<empty>");
        return;
    }

    for (const auto& label : labels)
        appendAsciiString (parameter, label);
}
} // namespace

tresult PLUGIN_API CVGMInstrumentLiteController::initialize (FUnknown* context)
{
    const tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    auto* soundFontParameter = new Vst::StringListParameter (STR16 ("SoundFont"), kParamGMSoundFont);
    appendAsciiString (*soundFontParameter, "Scanning CV_GM_Instrument_Lite_SoundFonts...");
    parameters.addParameter (soundFontParameter);

    auto* instrumentParameter = new Vst::StringListParameter (STR16 ("Instrument"), kParamGMInstrument);
    appendAsciiString (*instrumentParameter, "Select a SoundFont first");
    parameters.addParameter (instrumentParameter);

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Rescan"), kParamGMRescan, STR16 (""), 0.0, 1.0, 0.0, 1));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Volume"), kParamGMVolume, STR16 ("%"), 0.0, 1.0, kVolumeDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Bass"), kParamGMBass, STR16 ("dB"), -12.0, 12.0, kEqDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Mid"), kParamGMMid, STR16 ("dB"), -12.0, 12.0, kEqDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Treble"), kParamGMTreble, STR16 ("dB"), -12.0, 12.0, kEqDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Room"), kParamGMRoom, STR16 ("%"), 0.0, 1.0, kRoomDefault));

    initializeDynamicLists ();

    return result;
}

void CVGMInstrumentLiteController::initializeDynamicLists ()
{
    soundFontsFolder_ = resolveControllerSoundFontFolder ();
    previewEngine_.prepare (44100.0);
    refreshSoundFontList ();
    refreshPresetList ();
}

void CVGMInstrumentLiteController::refreshSoundFontList ()
{
    const auto currentIndex = selectedSoundFontIndexFromParameter ();
    const auto currentFileName = currentIndex < soundFontFiles_.size() ? soundFontFiles_[currentIndex].fileName : std::string {};

    soundFontsFolder_ = resolveControllerSoundFontFolder ();
    soundFontFiles_ = SoundFontFolderScanner::scanSoundFonts (soundFontsFolder_);
    rebuildSoundFontParameterStrings ();

    if (soundFontFiles_.empty())
    {
        EditControllerEx1::setParamNormalized (kParamGMSoundFont, 0.0);
        return;
    }

    auto selectedIndex = std::min (currentIndex, soundFontFiles_.size() - 1);
    if (!currentFileName.empty())
    {
        for (std::size_t index = 0; index < soundFontFiles_.size(); ++index)
        {
            if (soundFontFiles_[index].fileName == currentFileName)
            {
                selectedIndex = index;
                break;
            }
        }
    }

    EditControllerEx1::setParamNormalized (kParamGMSoundFont, listIndexToNormalized (selectedIndex, soundFontFiles_.size()));
}

void CVGMInstrumentLiteController::refreshPresetList ()
{
    const auto soundFontIndex = selectedSoundFontIndexFromParameter ();
    if (soundFontFiles_.empty() || soundFontIndex >= soundFontFiles_.size())
    {
        previewEngine_.unload ();
        rebuildInstrumentParameterStrings ();
        EditControllerEx1::setParamNormalized (kParamGMInstrument, 0.0);
        return;
    }

    if (!previewEngine_.isLoaded() || previewEngine_.loadedPath() != soundFontFiles_[soundFontIndex].path)
        (void)previewEngine_.loadSoundFont (soundFontFiles_[soundFontIndex].path);

    rebuildInstrumentParameterStrings ();

    const auto presetCount = previewEngine_.presets().size();
    if (presetCount == 0)
        EditControllerEx1::setParamNormalized (kParamGMInstrument, 0.0);
    else
        EditControllerEx1::setParamNormalized (kParamGMInstrument, listIndexToNormalized (std::min (selectedPresetIndexFromParameter (), presetCount - 1), presetCount));
}

void CVGMInstrumentLiteController::rebuildSoundFontParameterStrings ()
{
    if (auto* parameter = getStringListParameter (*this, kParamGMSoundFont))
    {
        std::vector<std::string> labels;
        labels.reserve (soundFontFiles_.size());
        for (const auto& soundFont : soundFontFiles_)
            labels.push_back (soundFont.fileName);

        if (labels.empty())
            labels.push_back ("No .sf2 found in CV_GM_Instrument_Lite_SoundFonts");

        replaceStringList (*parameter, labels);
    }
}

void CVGMInstrumentLiteController::rebuildInstrumentParameterStrings ()
{
    if (auto* parameter = getStringListParameter (*this, kParamGMInstrument))
    {
        std::vector<std::string> labels;
        const auto& presets = previewEngine_.presets();
        labels.reserve (presets.size());
        for (const auto& preset : presets)
            labels.push_back (preset.displayName);

        if (labels.empty())
            labels.push_back (previewEngine_.isLoaded() ? "No presets found in selected .sf2" : "Load a SoundFont to list real presets");

        replaceStringList (*parameter, labels);
    }
}

std::size_t CVGMInstrumentLiteController::selectedSoundFontIndexFromParameter () const noexcept
{
    return normalizedToListIndex (const_cast<CVGMInstrumentLiteController*> (this)->EditControllerEx1::getParamNormalized (kParamGMSoundFont), soundFontFiles_.size());
}

std::size_t CVGMInstrumentLiteController::selectedPresetIndexFromParameter () const noexcept
{
    return normalizedToListIndex (const_cast<CVGMInstrumentLiteController*> (this)->EditControllerEx1::getParamNormalized (kParamGMInstrument), previewEngine_.presets().size());
}

tresult PLUGIN_API CVGMInstrumentLiteController::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API CVGMInstrumentLiteController::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float volume = static_cast<float> (kVolumeDefault);
    float bass = static_cast<float> (kEqDefault);
    float mid = static_cast<float> (kEqDefault);
    float treble = static_cast<float> (kEqDefault);
    float room = static_cast<float> (kRoomDefault);
    float soundFont = 0.0f;
    float instrument = 0.0f;

    (void)streamer.readFloat (volume);
    (void)streamer.readFloat (bass);
    (void)streamer.readFloat (mid);
    (void)streamer.readFloat (treble);
    (void)streamer.readFloat (room);
    (void)streamer.readFloat (soundFont);
    (void)streamer.readFloat (instrument);

    setParamNormalized (kParamGMSoundFont, std::clamp<double> (soundFont, 0.0, 1.0));
    setParamNormalized (kParamGMInstrument, std::clamp<double> (instrument, 0.0, 1.0));
    setParamNormalized (kParamGMRescan, 0.0);
    setParamNormalized (kParamGMVolume, std::clamp<double> (volume, 0.0, 1.0));
    setParamNormalized (kParamGMBass, normalizeEq (bass));
    setParamNormalized (kParamGMMid, normalizeEq (mid));
    setParamNormalized (kParamGMTreble, normalizeEq (treble));
    setParamNormalized (kParamGMRoom, std::clamp<double> (room, 0.0, 1.0));
    return kResultOk;
}

tresult PLUGIN_API CVGMInstrumentLiteController::setState (IBStream* state)
{
    return state ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API CVGMInstrumentLiteController::getState (IBStream* state)
{
    return state ? kResultTrue : kResultFalse;
}


Vst::ParamValue PLUGIN_API CVGMInstrumentLiteController::normalizedParamToPlain (Vst::ParamID tag, Vst::ParamValue valueNormalized)
{
    return EditControllerEx1::normalizedParamToPlain (tag, valueNormalized);
}

Vst::ParamValue PLUGIN_API CVGMInstrumentLiteController::plainParamToNormalized (Vst::ParamID tag, Vst::ParamValue plainValue)
{
    return EditControllerEx1::plainParamToNormalized (tag, plainValue);
}

tresult PLUGIN_API CVGMInstrumentLiteController::setParamNormalized (Vst::ParamID tag, Vst::ParamValue value)
{
    const auto result = EditControllerEx1::setParamNormalized (tag, value);

    if (tag == kParamGMSoundFont)
    {
        EditControllerEx1::setParamNormalized (kParamGMInstrument, 0.0);
        refreshPresetList ();
    }
    else if (tag == kParamGMRescan && value > 0.5)
    {
        refreshSoundFontList ();
        refreshPresetList ();
    }

    return result;
}

tresult PLUGIN_API CVGMInstrumentLiteController::getMidiControllerAssignment (int32 busIndex,
                                                                               int16 /*channel*/,
                                                                               Vst::CtrlNumber midiControllerNumber,
                                                                               Vst::ParamID& id)
{
    if (busIndex != 0)
        return kResultFalse;

    switch (midiControllerNumber)
    {
        case Vst::kCtrlVolume:
            id = kParamGMVolume;
            return kResultTrue;

        case Vst::kCtrlEff1Depth:
            id = kParamGMRoom;
            return kResultTrue;

        case Vst::kCtrlSoundVariation:
            id = kParamGMBass;
            return kResultTrue;

        case Vst::kCtrlFilterCutoff:
            id = kParamGMMid;
            return kResultTrue;

        case Vst::kCtrlFilterResonance:
            id = kParamGMTreble;
            return kResultTrue;

        default:
            return kResultFalse;
    }
}

IPlugView* PLUGIN_API CVGMInstrumentLiteController::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if CV_GM_INSTRUMENT_LITE_ENABLE_CV_GUI
    Steinberg::ViewRect size (0, 0, 860, 560);
    return new CV::GUI::VST3ImGuiView (size, this, "CV GM Instrument Lite");
#else
    return nullptr;
#endif
}

} // namespace CV
