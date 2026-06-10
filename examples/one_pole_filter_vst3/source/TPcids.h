//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kOnePoleFilterVST3ProcessorUID (0x8f126ee8, 0x2d9c66e2, 0x327bc2e3, 0x5f369328);
static const Steinberg::FUID kOnePoleFilterVST3ControllerUID (0x5a4bf386, 0x2f565c74, 0x5691afd7, 0xcfbd5426);

#define OnePoleFilterVST3Category "Fx|Filter"

enum OnePoleFilterVST3ParamID : Steinberg::Vst::ParamID
{
    kParamOnePoleMode = 9400,
    kParamOnePoleCutoff
};

//------------------------------------------------------------------------
} // namespace CV
