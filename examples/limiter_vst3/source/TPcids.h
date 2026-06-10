//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kLimiterVST3ProcessorUID (0x735517de, 0x12774f57, 0xab153358, 0x2f879c1f);
static const Steinberg::FUID kLimiterVST3ControllerUID (0xdf94b713, 0x63604f42, 0x9a847f0e, 0xe7eb158f);

#define LimiterVST3Category "Fx|Dynamics"

//------------------------------------------------------------------------
} // namespace CV
namespace CV {
enum LimiterParamID : Steinberg::Vst::ParamID
{
    kParamLimiterThreshold = 5000,
    kParamLimiterRelease,
    kParamLimiterOutputGain
};
} // namespace CV
