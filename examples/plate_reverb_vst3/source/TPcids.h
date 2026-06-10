//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kPlateReverbVST3ProcessorUID (0xBDF93042, 0x6F1548BB, 0x8C99E167, 0xAC5B3303);
static const Steinberg::FUID kPlateReverbVST3ControllerUID (0x65B8B86F, 0x77A54536, 0x9D6A27AE, 0x975FD303);

#define PlateReverbVST3Category "Fx|Reverb"

enum PlateReverbVST3ParamID : Steinberg::Vst::ParamID
{
    kParamMix = 9300,
    kParamDecay,
    kParamDamping,
    kParamPreDelay
};

//------------------------------------------------------------------------
} // namespace CV
