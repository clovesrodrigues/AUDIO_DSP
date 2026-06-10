//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kPowerAmpVST3ProcessorUID (0x5d938f2c, 0xdd744d28, 0x8f1c93a9, 0x33facf03);
static const Steinberg::FUID kPowerAmpVST3ControllerUID (0x4cb8ea55, 0xf28e4db0, 0xa2049a28, 0x12e568ac);

#define PowerAmpVST3Category "Fx|Guitar"

enum PowerAmpVST3ParamID : Steinberg::Vst::ParamID
{
    kParamPowerAmpInputGain = 9600,
    kParamPowerAmpModel,
    kParamPowerAmpOutputGain
};

//------------------------------------------------------------------------
} // namespace CV
