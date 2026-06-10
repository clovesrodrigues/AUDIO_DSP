//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kSuperReverbVST3ProcessorUID (0x0B7A3E14, 0x55CE42C0, 0xBE973D42, 0x5A259704);
static const Steinberg::FUID kSuperReverbVST3ControllerUID (0x9DE2F77B, 0xBA4B4C40, 0x9894019E, 0x2BD09704);

#define SuperReverbVST3Category "Fx|Reverb"

enum SuperReverbVST3ParamID : Steinberg::Vst::ParamID
{
    kParamMix = 9700,
    kParamDwell,
    kParamTone
};

//------------------------------------------------------------------------
} // namespace CV
