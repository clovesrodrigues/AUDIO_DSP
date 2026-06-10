//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kEnvelopeFollowerVST3ProcessorUID (0x6db02a1a, 0x7f344786, 0x9c2729c8, 0xd63c24a1);
static const Steinberg::FUID kEnvelopeFollowerVST3ControllerUID (0x84b8cf20, 0x412640e2, 0xa48202fe, 0x0da4aa8b);

#define EnvelopeFollowerVST3Category "Fx|Dynamics"

//------------------------------------------------------------------------
} // namespace CV
namespace CV {
enum EnvelopeFollowerParamID : Steinberg::Vst::ParamID
{
    kParamEnvelopeMode = 3000,
    kParamEnvelopeAttack,
    kParamEnvelopeRelease,
    kParamEnvelopeOutputGain,
    kParamEnvelopeMix
};

enum EnvelopeFollowerModeIndex : Steinberg::int32
{
    kEnvelopeModePeak = 0,
    kEnvelopeModeRMS,
    kEnvelopeModeCount
};
} // namespace CV
