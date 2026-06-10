//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kAllPassFilterVST3ProcessorUID (0x46874899, 0xd767bd2e, 0x6e4f8800, 0x752bb35e);
static const Steinberg::FUID kAllPassFilterVST3ControllerUID (0x30f4b1a4, 0xb57ea894, 0xe3525291, 0x6d769766);

#define AllPassFilterVST3Category "Fx|Filter"

enum AllPassFilterVST3ParamID : Steinberg::Vst::ParamID
{
    kParamAllPassDelaySamples = 9000,
    kParamAllPassFeedback
};

//------------------------------------------------------------------------
} // namespace CV
