//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kTubePreampVST3ProcessorUID (0x6ab53f91, 0x9c3b4518, 0x9e69187a, 0x2e02ac5f);
static const Steinberg::FUID kTubePreampVST3ControllerUID (0xaf4bbfb5, 0xa0a041f3, 0x93378822, 0xd53a995a);

#define TubePreampVST3Category "Fx|Guitar"

enum TubePreampVST3ParamID : Steinberg::Vst::ParamID
{
    kParamTubePreampDrive = 9500,
    kParamTubePreampBias,
    kParamTubePreampPlateVoltage,
    kParamTubePreampStages,
    kParamTubePreampOutputGain
};

//------------------------------------------------------------------------
} // namespace CV
