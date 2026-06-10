//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kFlangerVST3ProcessorUID (0xa37caf3e, 0xea063cdd, 0x7274fabd, 0x96d9b919);
static const Steinberg::FUID kFlangerVST3ControllerUID (0x22a4e78c, 0x43934edc, 0x37f10a43, 0x76a57a16);

#define FlangerVST3Category "Fx|Modulation"

enum FlangerVST3ParamID : Steinberg::Vst::ParamID
{
    kParamFlangerRate = 8000,
    kParamFlangerDepth,
    kParamFlangerFeedback,
    kParamFlangerMix
};

//------------------------------------------------------------------------
} // namespace CV
