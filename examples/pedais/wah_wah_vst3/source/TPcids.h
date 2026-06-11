//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {

static const Steinberg::FUID kWahWahVST3ProcessorUID (0x42e91b7c, 0xa0364f5e, 0x8b2d7194, 0xc5a0e63f);
static const Steinberg::FUID kWahWahVST3ControllerUID (0x90c4d13a, 0x2f8e4b71, 0xad359026, 0x6b7c18f2);

#define WahWahVST3Category "Fx|Filter"

} // namespace CV
