//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kMarshallToneVST3ProcessorUID (0x4e707390, 0x529a4d82, 0x85dc156a, 0x5a809535);
static const Steinberg::FUID kMarshallToneVST3ControllerUID (0x553fd0f9, 0xe48a45dc, 0x846c83af, 0x9f219c4e);

#define MarshallToneVST3Category "Fx|Guitar|Tone Stack"

enum MarshallToneVST3ParamID : Steinberg::Vst::ParamID
{
    kParamMarshallToneBass = 9800,
    kParamMarshallToneMid,
    kParamMarshallToneTreble
};

//------------------------------------------------------------------------
} // namespace CV
