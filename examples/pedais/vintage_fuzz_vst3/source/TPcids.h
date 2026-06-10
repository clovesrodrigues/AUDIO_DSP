//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {

static const Steinberg::FUID kVintageFuzzVST3ProcessorUID (0x49763e78, 0xc9ea1600, 0x6384cf61, 0xd772467a);
static const Steinberg::FUID kVintageFuzzVST3ControllerUID (0x5f7079f6, 0x98c8733a, 0x924d00db, 0xc7229be4);

#define VintageFuzzVST3Category "Fx|Distortion"

} // namespace CV
