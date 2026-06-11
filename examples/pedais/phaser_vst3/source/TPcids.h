//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {

static const Steinberg::FUID kPhaserVST3ProcessorUID (0x0f9b62e4, 0x51d341ac, 0x83c2a7e9, 0x5b18d604);
static const Steinberg::FUID kPhaserVST3ControllerUID (0x7d2a4c98, 0xe13f40b2, 0x9c6e2571, 0xa8f3d0b5);

#define PhaserVST3Category "Fx|Modulation"

} // namespace CV
