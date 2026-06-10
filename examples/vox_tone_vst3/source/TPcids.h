//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kVoxToneVST3ProcessorUID (0x4f4a6f2e, 0x21db4d49, 0x9816450f, 0x456cfd4a);
static const Steinberg::FUID kVoxToneVST3ControllerUID (0xad6255c4, 0x2afc4aa2, 0xa4f5789c, 0x91a24d0b);

#define VoxToneVST3Category "Fx|Guitar|Tone Stack"

enum VoxToneVST3ParamID : Steinberg::Vst::ParamID
{
    kParamVoxToneBass = 9900,
    kParamVoxToneMid,
    kParamVoxToneTreble
};

//------------------------------------------------------------------------
} // namespace CV
