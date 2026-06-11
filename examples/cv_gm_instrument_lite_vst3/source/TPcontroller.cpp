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
#include <array>
#include <filesystem>
#include <string>

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

constexpr std::array<const char*, 128> kGeneralMidiPresetNames {{
    "000 Acoustic Grand Piano", "001 Bright Acoustic Piano", "002 Electric Grand Piano", "003 Honky-tonk Piano",
    "004 Electric Piano 1", "005 Electric Piano 2", "006 Harpsichord", "007 Clavinet",
    "008 Celesta", "009 Glockenspiel", "010 Music Box", "011 Vibraphone",
    "012 Marimba", "013 Xylophone", "014 Tubular Bells", "015 Dulcimer",
    "016 Drawbar Organ", "017 Percussive Organ", "018 Rock Organ", "019 Church Organ",
    "020 Reed Organ", "021 Accordion", "022 Harmonica", "023 Tango Accordion",
    "024 Acoustic Guitar Nylon", "025 Acoustic Guitar Steel", "026 Electric Guitar Jazz", "027 Electric Guitar Clean",
    "028 Electric Guitar Muted", "029 Overdriven Guitar", "030 Distortion Guitar", "031 Guitar Harmonics",
    "032 Acoustic Bass", "033 Electric Bass Finger", "034 Electric Bass Pick", "035 Fretless Bass",
    "036 Slap Bass 1", "037 Slap Bass 2", "038 Synth Bass 1", "039 Synth Bass 2",
    "040 Violin", "041 Viola", "042 Cello", "043 Contrabass",
    "044 Tremolo Strings", "045 Pizzicato Strings", "046 Orchestral Harp", "047 Timpani",
    "048 String Ensemble 1", "049 String Ensemble 2", "050 Synth Strings 1", "051 Synth Strings 2",
    "052 Choir Aahs", "053 Voice Oohs", "054 Synth Voice", "055 Orchestra Hit",
    "056 Trumpet", "057 Trombone", "058 Tuba", "059 Muted Trumpet",
    "060 French Horn", "061 Brass Section", "062 Synth Brass 1", "063 Synth Brass 2",
    "064 Soprano Sax", "065 Alto Sax", "066 Tenor Sax", "067 Baritone Sax",
    "068 Oboe", "069 English Horn", "070 Bassoon", "071 Clarinet",
    "072 Piccolo", "073 Flute", "074 Recorder", "075 Pan Flute",
    "076 Blown Bottle", "077 Shakuhachi", "078 Whistle", "079 Ocarina",
    "080 Lead 1 Square", "081 Lead 2 Sawtooth", "082 Lead 3 Calliope", "083 Lead 4 Chiff",
    "084 Lead 5 Charang", "085 Lead 6 Voice", "086 Lead 7 Fifths", "087 Lead 8 Bass + Lead",
    "088 Pad 1 New Age", "089 Pad 2 Warm", "090 Pad 3 Polysynth", "091 Pad 4 Choir",
    "092 Pad 5 Bowed", "093 Pad 6 Metallic", "094 Pad 7 Halo", "095 Pad 8 Sweep",
    "096 FX 1 Rain", "097 FX 2 Soundtrack", "098 FX 3 Crystal", "099 FX 4 Atmosphere",
    "100 FX 5 Brightness", "101 FX 6 Goblins", "102 FX 7 Echoes", "103 FX 8 Sci-fi",
    "104 Sitar", "105 Banjo", "106 Shamisen", "107 Koto",
    "108 Kalimba", "109 Bagpipe", "110 Fiddle", "111 Shanai",
    "112 Tinkle Bell", "113 Agogo", "114 Steel Drums", "115 Woodblock",
    "116 Taiko Drum", "117 Melodic Tom", "118 Synth Drum", "119 Reverse Cymbal",
    "120 Guitar Fret Noise", "121 Breath Noise", "122 Seashore", "123 Bird Tweet",
    "124 Telephone Ring", "125 Helicopter", "126 Applause", "127 Gunshot"
}};

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

std::filesystem::path resolveControllerSoundFontFolder ()
{
    return SoundFontFolderScanner::resolveSiblingSoundFontsFolder (std::filesystem::current_path() / "CV_GM_Instrument_Lite.vst3");
}
} // namespace

tresult PLUGIN_API CVGMInstrumentLiteController::initialize (FUnknown* context)
{
    const tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    auto* soundFontParameter = new Vst::StringListParameter (STR16 ("SoundFont"), kParamGMSoundFont);
    const auto soundFonts = SoundFontFolderScanner::scanSoundFonts (resolveControllerSoundFontFolder ());
    if (soundFonts.empty())
        appendAsciiString (*soundFontParameter, "No .sf2 found in CV_GM_Instrument_Lite_SoundFonts");
    else
    {
        for (const auto& soundFont : soundFonts)
            appendAsciiString (*soundFontParameter, soundFont.fileName);
    }
    parameters.addParameter (soundFontParameter);

    auto* instrumentParameter = new Vst::StringListParameter (STR16 ("Instrument"), kParamGMInstrument);
    for (const auto* presetName : kGeneralMidiPresetNames)
        appendAsciiString (*instrumentParameter, presetName);
    parameters.addParameter (instrumentParameter);

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Rescan"), kParamGMRescan, STR16 (""), 0.0, 1.0, 0.0, 1));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Volume"), kParamGMVolume, STR16 ("%"), 0.0, 1.0, kVolumeDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Bass"), kParamGMBass, STR16 ("dB"), -12.0, 12.0, kEqDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Mid"), kParamGMMid, STR16 ("dB"), -12.0, 12.0, kEqDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Treble"), kParamGMTreble, STR16 ("dB"), -12.0, 12.0, kEqDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Room"), kParamGMRoom, STR16 ("%"), 0.0, 1.0, kRoomDefault));

    return result;
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
    Steinberg::ViewRect size (0, 0, 520, 360);
    return new CV::GUI::VST3ImGuiView (size, this, "CV GM Instrument Lite");
#else
    return nullptr;
#endif
}

} // namespace CV
