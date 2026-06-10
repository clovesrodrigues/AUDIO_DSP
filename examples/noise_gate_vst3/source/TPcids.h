//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kNoiseGateVST3ProcessorUID (0x3bbbbfee, 0xea04ef77, 0x12390d47, 0x0c4b0444);
static const Steinberg::FUID kNoiseGateVST3ControllerUID (0xc441e0be, 0x04e7b73b, 0x5171eb99, 0xaa5c38af);

#define NoiseGateVST3Category "Fx|Dynamics"

enum NoiseGateVST3ParamID : Steinberg::Vst::ParamID
{
    kParamNoiseGateThresholdOpen = 6000,
    kParamNoiseGateThresholdClose,
    kParamNoiseGateAttack,
    kParamNoiseGateHold,
    kParamNoiseGateRelease
};

//------------------------------------------------------------------------
} // namespace CV
