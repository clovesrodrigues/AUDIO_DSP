//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kFFTVST3ProcessorUID (0xb8d6f8ad, 0xd6184e4f, 0x9a23523a, 0x75f4e817);
static const Steinberg::FUID kFFTVST3ControllerUID (0x42fb0640, 0x46b24d82, 0xb8f487f0, 0x2210d733);

#define FFTVST3Category "Fx|Analyzer|Spectral"

enum FFTVST3ParamID : Steinberg::Vst::ParamID
{
    kParamFFTVST3Window = 10000,
    kParamFFTVST3FFTSize
};

//------------------------------------------------------------------------
} // namespace CV
