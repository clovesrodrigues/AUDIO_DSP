//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kHallReverbVST3ProcessorUID (0x2F184E60, 0x9A104CB6, 0xAFD08B2A, 0x7C1D2202);
static const Steinberg::FUID kHallReverbVST3ControllerUID (0x783E77BC, 0x24144B78, 0x95BF3D1E, 0xE74AC202);

#define HallReverbVST3Category "Fx|Reverb"

enum HallReverbVST3ParamID : Steinberg::Vst::ParamID
{
    kParamMix = 9200,
    kParamDecay,
    kParamDamping,
    kParamPreDelay
};

//------------------------------------------------------------------------
} // namespace CV
