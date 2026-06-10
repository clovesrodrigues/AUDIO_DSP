//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kLadderFilterVST3ProcessorUID (0xd5951ba5, 0xdae1d9ab, 0x6e56cc1a, 0x5d811dea);
static const Steinberg::FUID kLadderFilterVST3ControllerUID (0x21500f7c, 0x0b3f317a, 0x32e557ed, 0x1f3656d5);

#define LadderFilterVST3Category "Fx|Filter"

enum LadderFilterVST3ParamID : Steinberg::Vst::ParamID
{
    kParamLadderCutoff = 9300,
    kParamLadderResonance,
    kParamLadderDrive
};

//------------------------------------------------------------------------
} // namespace CV
