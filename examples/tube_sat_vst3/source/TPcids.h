//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kTubeSatVST3ProcessorUID (0x5e1e5a41, 0x8b7a4d5e, 0xa4671d22, 0x64206190);
static const Steinberg::FUID kTubeSatVST3ControllerUID (0x7014b8ca, 0x0dfd41d1, 0xa4bb308d, 0x8b20a4e1);

#define TubeSatVST3Category "Fx|Saturation"

enum TubeSatVST3ParamID : Steinberg::Vst::ParamID
{
    kParamTubeSatDrive = 9200,
    kParamTubeSatBias,
    kParamTubeSatMix
};

//------------------------------------------------------------------------
} // namespace CV
