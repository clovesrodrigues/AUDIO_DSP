//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kMidSideVST3ProcessorUID (0x512e90d7, 0x6030077b, 0x280cd81f, 0x5d11d619);
static const Steinberg::FUID kMidSideVST3ControllerUID (0xb1e3d8f6, 0x5d8f3f1c, 0xa29b1362, 0xa4e74c7a);

#define MidSideVST3Category "Fx|Spatial"

enum MidSideVST3ParamID : Steinberg::Vst::ParamID
{
    kParamMidSideVST3MidGain = 9800,
    kParamMidSideVST3SideGain,
    kParamMidSideVST3Width
};

//------------------------------------------------------------------------
} // namespace CV
