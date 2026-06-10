//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kGraphicEQVST3ProcessorUID (0xa5f0fcf1, 0x37144554, 0x8241f378, 0xd26aa72f);
static const Steinberg::FUID kGraphicEQVST3ControllerUID (0xbc116b23, 0x6b574ed1, 0x87d331ee, 0xa1d39ab8);

#define GraphicEQVST3Category "Fx|EQ"

constexpr Steinberg::Vst::ParamID kGraphicEQBandCount = 10;
constexpr Steinberg::Vst::ParamID kGraphicEQBaseParamID = 2000;

constexpr Steinberg::Vst::ParamID graphicEQGainParamID (Steinberg::Vst::ParamID band) noexcept
{
    return kGraphicEQBaseParamID + band;
}

//------------------------------------------------------------------------
} // namespace CV
