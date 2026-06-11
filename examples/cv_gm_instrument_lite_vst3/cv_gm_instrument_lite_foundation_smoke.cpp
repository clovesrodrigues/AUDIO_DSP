#include "source/TPcids.h"
#include "source/SoundFontFolderScanner.h"
#include "source/SoundFontEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

int main()
{
    assert(std::strcmp(CV::kCVGMInstrumentLiteSoundFontsFolderName, "CV_GM_Instrument_Lite_SoundFonts") == 0);
    assert(std::strcmp(CV::kCVGMInstrumentLiteDataFolderName, "_CV_GM_Instrument_Lite_Data") == 0);
    assert(CV::kParamGMVolume != CV::kParamGMRoom);

    const auto fakeBundle = std::filesystem::temp_directory_path() / "CV_GM_Instrument_Lite.vst3" / "Contents" / "x86_64-linux" / "CV_GM_Instrument_Lite.so";
    const auto resolvedFolder = CV::SoundFontFolderScanner::resolveSiblingSoundFontsFolder(fakeBundle);
    assert(resolvedFolder.filename() == CV::kCVGMInstrumentLiteSoundFontsFolderName);
    assert(resolvedFolder.parent_path() == std::filesystem::temp_directory_path());

    const auto scanRoot = std::filesystem::temp_directory_path() / "cv_gm_instrument_lite_scanner_smoke";
    std::filesystem::remove_all(scanRoot);
    std::filesystem::create_directories(scanRoot);
    {
        std::ofstream(scanRoot / "z_last.sf2").put('z');
        std::ofstream(scanRoot / "A_first.SF2").put('a');
        std::ofstream(scanRoot / "ignore.txt").put('x');
    }

    const auto files = CV::SoundFontFolderScanner::scanSoundFonts(scanRoot);
    assert(files.size() == 2);
    assert(files[0].fileName == "A_first.SF2");
    assert(files[1].fileName == "z_last.sf2");
    assert(CV::SoundFontFolderScanner::resolveDataFolder(scanRoot).filename() == CV::kCVGMInstrumentLiteDataFolderName);
    std::filesystem::remove_all(scanRoot);

    assert(CV::SoundFontEngine::formatPresetDisplayName(0, 33, "Electric Bass Finger") == "000:033 Electric Bass Finger");
    assert(CV::SoundFontEngine::formatPresetDisplayName(128, 0, "Standard Drum Kit") == "128:000 Standard Drum Kit");

    CV::SoundFontEngine engine;
    engine.prepare(48000.0);
    engine.setPitchBend(0.25f);
    assert(std::abs(engine.pitchBend() - 0.25f) < 0.0001f);
    engine.setPitchBend(2.0f);
    assert(std::abs(engine.pitchBend() - 1.0f) < 0.0001f);
    engine.setPitchBend(-2.0f);
    assert(std::abs(engine.pitchBend() + 1.0f) < 0.0001f);
    engine.setPitchBend(0.0f);
    assert(std::abs(engine.pitchBend()) < 0.0001f);
    engine.setMidiController(64, 127);
    engine.allNotesOff();

#if CV_GM_INSTRUMENT_LITE_HAS_TINYSOUNDFONT
    assert(CV::SoundFontEngine::isBackendAvailable());
    if (const char* soundFontPath = std::getenv("CV_GM_INSTRUMENT_LITE_TEST_SF2"))
    {
        if (engine.loadSoundFont(soundFontPath))
        {
            assert(engine.isLoaded());
            assert(!engine.presets().empty());
            assert(!engine.presets().front().displayName.empty());
            assert(engine.selectPresetByListIndex(0));
            assert(engine.selectPresetByIndex(engine.presets().front().presetIndex));
            engine.noteOn(60, 0.8f);

            std::vector<float> output(512 * 2, 0.0f);
            engine.renderInterleavedStereo(output.data(), 512, false);
            const auto peak = *std::max_element(output.begin(), output.end(), [] (float left, float right) {
                return std::abs(left) < std::abs(right);
            });
            assert(std::isfinite(peak));
            engine.noteOff(60);
            engine.allNotesOff();
        }
    }
#else
    assert(!CV::SoundFontEngine::isBackendAvailable());
    assert(!engine.loadSoundFont(scanRoot / "missing.sf2"));
#endif

    return 0;
}
