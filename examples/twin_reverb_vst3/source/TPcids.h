//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kTwinReverbVST3ProcessorUID (0xA0E5F212, 0x9CC64A21, 0x8A2BD544, 0x8E739502);
static const Steinberg::FUID kTwinReverbVST3ControllerUID (0x577E3F9D, 0x8E184B57, 0x9BC0A861, 0x11D09502);

#define TwinReverbVST3Category "Fx|Reverb"

enum TwinReverbVST3ParamID : Steinberg::Vst::ParamID
{
    kParamMix = 9500,
    kParamDwell,
    kParamTone
};

//------------------------------------------------------------------------
} // namespace CV
