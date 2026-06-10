//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kDeluxeReverbVST3ProcessorUID (0x522EE313, 0xBDE24C9E, 0x93C3E678, 0x0E3E9603);
static const Steinberg::FUID kDeluxeReverbVST3ControllerUID (0xC3EB221F, 0xC63D426D, 0xAF038E25, 0xFBAB9603);

#define DeluxeReverbVST3Category "Fx|Reverb"

enum DeluxeReverbVST3ParamID : Steinberg::Vst::ParamID
{
    kParamMix = 9600,
    kParamDwell,
    kParamTone
};

//------------------------------------------------------------------------
} // namespace CV
