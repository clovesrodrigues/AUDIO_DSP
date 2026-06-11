//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kCVGMInstrumentLiteProcessorUID (0xd9893328, 0x89c2d60a, 0xaffd60bc, 0xb1ccedbf);
static const Steinberg::FUID kCVGMInstrumentLiteControllerUID (0x479d2e4d, 0x7082aac8, 0x81cf8c73, 0xd22489f5);

#define CVGMInstrumentLiteCategory "Instrument|GM|SoundFont"

inline constexpr const char* kCVGMInstrumentLiteSoundFontsFolderName = "CV_GM_Instrument_Lite_SoundFonts";
inline constexpr const char* kCVGMInstrumentLiteDataFolderName = "_CV_GM_Instrument_Lite_Data";

constexpr Steinberg::Vst::ParamID kParamGMSoundFont = 13997;
constexpr Steinberg::Vst::ParamID kParamGMInstrument = 13998;
constexpr Steinberg::Vst::ParamID kParamGMRescan = 13999;
constexpr Steinberg::Vst::ParamID kParamGMVolume = 14000;
constexpr Steinberg::Vst::ParamID kParamGMBass = 14001;
constexpr Steinberg::Vst::ParamID kParamGMMid = 14002;
constexpr Steinberg::Vst::ParamID kParamGMTreble = 14003;
constexpr Steinberg::Vst::ParamID kParamGMRoom = 14004;

//------------------------------------------------------------------------
} // namespace CV
