//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kReverbSuiteVST3ProcessorUID (0xA9126F3B, 0x80F64DF5, 0xB4F3A3A1, 0x75E71920);
static const Steinberg::FUID kReverbSuiteVST3ControllerUID (0xCB761B04, 0x53A8429C, 0x9D6F7413, 0xAE39C650);

#define ReverbSuiteVST3Category "Fx|Reverb"

enum ReverbSuiteVST3ParamID : Steinberg::Vst::ParamID
{
    kParamAlgorithm = 9000,
    kParamMix,
    kParamDecay,
    kParamTone,
    kParamPreDelay,
    kParamWidthDwell
};

//------------------------------------------------------------------------
} // namespace CV
