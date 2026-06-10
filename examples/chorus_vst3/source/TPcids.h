//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kChorusVST3ProcessorUID (0x39b28f27, 0x99b59042, 0x14e2a8b6, 0xbedf9d6a);
static const Steinberg::FUID kChorusVST3ControllerUID (0x0e3515f2, 0x43fa32d4, 0xd078dbb2, 0xc34b4769);

#define ChorusVST3Category "Fx|Modulation"

enum ChorusVST3ParamID : Steinberg::Vst::ParamID
{
    kParamChorusRate = 7000,
    kParamChorusDepth,
    kParamChorusMix
};

//------------------------------------------------------------------------
} // namespace CV
