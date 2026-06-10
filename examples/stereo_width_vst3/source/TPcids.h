//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kStereoWidthVST3ProcessorUID (0x0a816835, 0xf4f36a81, 0x3cf2118c, 0x04a38a69);
static const Steinberg::FUID kStereoWidthVST3ControllerUID (0xcaaf5045, 0xab509ba9, 0xbf02cdcf, 0x2d1e0e32);

#define StereoWidthVST3Category "Fx|Spatial"

enum StereoWidthVST3ParamID : Steinberg::Vst::ParamID
{
    kParamStereoWidthVST3MidGain = 9900,
    kParamStereoWidthVST3SideGain,
    kParamStereoWidthVST3Width
};

//------------------------------------------------------------------------
} // namespace CV
