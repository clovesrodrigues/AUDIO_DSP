#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {

static const Steinberg::FUID kGUITestProcessorUID (0x8b34a8c2, 0x2ae94f31, 0xa951ce47, 0xdd7dfc24);
static const Steinberg::FUID kGUITestControllerUID (0x3fa98779, 0xdb224aa2, 0x87bc00d5, 0xbc1c2dc1);

#define GUITestVST3Category "Fx|Tools"

enum GUITestParamID : Steinberg::Vst::ParamID
{
    kParamButtonCounter = 100,
    kParamSlider,
    kParamListbox,
    kParamKnob
};

} // namespace CV
