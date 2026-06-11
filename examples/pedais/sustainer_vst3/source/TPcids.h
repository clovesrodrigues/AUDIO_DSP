//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {

static const Steinberg::FUID kSustainerVST3ProcessorUID (0x1a7f3c21, 0xb6d04e1a, 0x9a2c5fb7, 0x7410d92e);
static const Steinberg::FUID kSustainerVST3ControllerUID (0x6c5d218f, 0x0e3a4bb9, 0xa6128fd4, 0x32b7c901);

#define SustainerVST3Category "Fx|Dynamics"

} // namespace CV
