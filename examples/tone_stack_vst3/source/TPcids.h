//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kToneStackVST3ProcessorUID (0xf1272a8a, 0x23474ed1, 0x965accaf, 0x35869c13);
static const Steinberg::FUID kToneStackVST3ControllerUID (0xdebbf7c1, 0xcadc44aa, 0xbfaf283c, 0x6a31221d);

#define ToneStackVST3Category "Fx|Guitar|Tone Stack"

enum ToneStackVST3ParamID : Steinberg::Vst::ParamID
{
    kParamToneStackBass = 9700,
    kParamToneStackMid,
    kParamToneStackTreble
};

//------------------------------------------------------------------------
} // namespace CV
