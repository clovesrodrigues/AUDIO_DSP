//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {

static const Steinberg::FUID kClassicOverdriveVST3ProcessorUID (0x8ec31a06, 0x5424241f, 0x837f47e7, 0x6cbccd99);
static const Steinberg::FUID kClassicOverdriveVST3ControllerUID (0x509f0352, 0x0df9c001, 0x925fe9ef, 0x519cd2ae);

#define ClassicOverdriveVST3Category "Fx|Distortion"

} // namespace CV
