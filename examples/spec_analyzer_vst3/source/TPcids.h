//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kSpectrumAnalyzerVST3ProcessorUID (0xd9ff9ae3, 0x627549fa, 0x8ed2f79d, 0x81226fbb);
static const Steinberg::FUID kSpectrumAnalyzerVST3ControllerUID (0x83f65cf7, 0xb9ad40e9, 0x840480a2, 0x489b9d27);

#define SpectrumAnalyzerVST3Category "Fx|Analyzer|Spectral"

enum SpectrumAnalyzerVST3ParamID : Steinberg::Vst::ParamID
{
    kParamSpectrumAnalyzerVST3Window = 10200,
    kParamSpectrumAnalyzerVST3FFTSize
};

//------------------------------------------------------------------------
} // namespace CV
