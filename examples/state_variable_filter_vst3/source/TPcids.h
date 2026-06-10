//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kStateVariableFilterVST3ProcessorUID (0xeceb95c7, 0x3ce62a38, 0x50ac8d97, 0x77bf3477);
static const Steinberg::FUID kStateVariableFilterVST3ControllerUID (0xecfa46e6, 0xe0897a7e, 0xb52004e8, 0xb3e3c24b);

#define StateVariableFilterVST3Category "Fx|Filter"

enum StateVariableFilterVST3ParamID : Steinberg::Vst::ParamID
{
    kParamSVFMode = 9500,
    kParamSVFCutoff,
    kParamSVFResonance
};

//------------------------------------------------------------------------
} // namespace CV
