//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kWaveshaperVST3ProcessorUID (0x4e092f63, 0x21e641aa, 0xa1ba2f7e, 0xd6491e18);
static const Steinberg::FUID kWaveshaperVST3ControllerUID (0xe8d2c8a3, 0x5a4b48d6, 0x8d9f7f83, 0x78e6a3c2);

#define WaveshaperVST3Category "Fx|Saturation"

enum WaveshaperVST3ParamID : Steinberg::Vst::ParamID
{
    kParamWaveshaperDrive = 9300,
    kParamWaveshaperBias,
    kParamWaveshaperMix
};

//------------------------------------------------------------------------
} // namespace CV
