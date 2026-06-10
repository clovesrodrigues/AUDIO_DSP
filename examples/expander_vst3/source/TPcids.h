//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kExpanderVST3ProcessorUID (0x1830ed55, 0x249c4e5d, 0x9762a481, 0x18bb1c19);
static const Steinberg::FUID kExpanderVST3ControllerUID (0x611a04f9, 0x3d9d4b2e, 0xb77d8db1, 0xc716e0de);

#define ExpanderVST3Category "Fx|Dynamics"

//------------------------------------------------------------------------
} // namespace CV
namespace CV {
enum ExpanderParamID : Steinberg::Vst::ParamID
{
    kParamExpanderThreshold = 4000,
    kParamExpanderRatio,
    kParamExpanderAttack,
    kParamExpanderRelease
};
} // namespace CV
