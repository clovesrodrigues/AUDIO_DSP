//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kSTFTVST3ProcessorUID (0x2092ad75, 0x285e4ca6, 0x8784effd, 0x7bd944a8);
static const Steinberg::FUID kSTFTVST3ControllerUID (0x5ba59e0c, 0xc3e2473f, 0x9177ef67, 0xd8d9172d);

#define STFTVST3Category "Fx|Analyzer|Spectral"

enum STFTVST3ParamID : Steinberg::Vst::ParamID
{
    kParamSTFTVST3Window = 10100,
    kParamSTFTVST3FFTSize
};

//------------------------------------------------------------------------
} // namespace CV
