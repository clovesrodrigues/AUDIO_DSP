//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kTapeSaturationVST3ProcessorUID (0x91a8f091, 0x5ee24ec2, 0xb6306f85, 0x12551d80);
static const Steinberg::FUID kTapeSaturationVST3ControllerUID (0x205d3d1a, 0x867d4859, 0x9df74d4e, 0xaef12301);

#define TapeSaturationVST3Category "Fx|Saturation"

enum TapeSaturationVST3ParamID : Steinberg::Vst::ParamID
{
    kParamTapeSaturationDrive = 9100,
    kParamTapeSaturationBias,
    kParamTapeSaturationMix
};

//------------------------------------------------------------------------
} // namespace CV
