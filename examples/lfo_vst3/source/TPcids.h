//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kLFOVST3ProcessorUID (0xd15d1b6a, 0x69ac4762, 0xa1b833dd, 0x7b1d3c88);
static const Steinberg::FUID kLFOVST3ControllerUID (0x7b632cb9, 0x9d044a1f, 0x8ea71512, 0x173c5d7b);

#define LFOVST3Category "Fx|Modulation"

enum LFOVST3ParamID : Steinberg::Vst::ParamID
{
    kParamLFOWaveform = 9700,
    kParamLFOFrequency
};

//------------------------------------------------------------------------
} // namespace CV
