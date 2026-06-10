//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kDCBlockerVST3ProcessorUID (0x465eb2a8, 0x0ca1cdaa, 0x04ded8a8, 0x1925530b);
static const Steinberg::FUID kDCBlockerVST3ControllerUID (0x452494a0, 0x772604fe, 0x0dd96f34, 0x18773f93);

#define DCBlockerVST3Category "Fx|Filter"

enum DCBlockerVST3ParamID : Steinberg::Vst::ParamID
{
    kParamDCBlockerCutoff = 9200
};

//------------------------------------------------------------------------
} // namespace CV
