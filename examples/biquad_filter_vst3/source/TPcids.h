//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kBiquadFilterVST3ProcessorUID (0xf6ed9cd1, 0x75370cee, 0xec8900e1, 0x4e796338);
static const Steinberg::FUID kBiquadFilterVST3ControllerUID (0xca7c94a5, 0xa1e72091, 0x6e5aed58, 0x0b332fb0);

#define BiquadFilterVST3Category "Fx|Filter"

enum BiquadFilterVST3ParamID : Steinberg::Vst::ParamID
{
    kParamBiquadMode = 9100,
    kParamBiquadFrequency,
    kParamBiquadQ,
    kParamBiquadGain
};

//------------------------------------------------------------------------
} // namespace CV
