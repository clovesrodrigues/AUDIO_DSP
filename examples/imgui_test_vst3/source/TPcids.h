//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kImGuiTestProcessorUID (0xa82af352, 0x1c884add, 0x8bcfc02a, 0xdf561853);
static const Steinberg::FUID kImGuiTestControllerUID (0x6f53c737, 0x16aa4451, 0x86d5ccca, 0x271866c1);

#define ImGuiTestVST3Category "Fx|Tools"

enum ImGuiTestParamID : Steinberg::Vst::ParamID
{
    kParamGuiSlider = 200,
    kParamGuiListBox,
    kParamButtonCounter
};

//------------------------------------------------------------------------
} // namespace CV
