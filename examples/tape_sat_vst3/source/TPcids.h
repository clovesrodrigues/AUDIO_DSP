//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kTapeSatVST3ProcessorUID (0x84232a43, 0x3771ed4c, 0x7a10c96c, 0xbde72eca);
static const Steinberg::FUID kTapeSatVST3ControllerUID (0xf68cf367, 0xe4e49841, 0x6f50896b, 0xb21ee4c1);

#define TapeSatVST3Category "Fx|Saturation"

enum TapeSatVST3ParamID : Steinberg::Vst::ParamID
{
    kParamTapeSatDrive = 9100,
    kParamTapeSatBias,
    kParamTapeSatMix
};

//------------------------------------------------------------------------
} // namespace CV
