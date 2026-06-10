//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kSpringReverbVST3ProcessorUID (0xD30A7E11, 0x3EA24C02, 0xA30F3C87, 0x43859401);
static const Steinberg::FUID kSpringReverbVST3ControllerUID (0x70F1BD4E, 0x042F47E5, 0xB3127A39, 0xF1649401);

#define SpringReverbVST3Category "Fx|Reverb"

enum SpringReverbVST3ParamID : Steinberg::Vst::ParamID
{
    kParamMix = 9400,
    kParamDwell,
    kParamTone
};

//------------------------------------------------------------------------
} // namespace CV
