//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kCompressorVST3ProcessorUID (0x095bf8fd, 0xb23d4795, 0x9d0c4ed7, 0xe52059bf);
static const Steinberg::FUID kCompressorVST3ControllerUID (0xad396937, 0x0a734029, 0xa388a380, 0xc732a3cb);

#define CompressorVST3Category "Fx|Dynamics"

enum CompressorParamID : Steinberg::Vst::ParamID
{
    kParamThreshold = 100,
    kParamRatio,
    kParamAttack,
    kParamRelease,
    kParamKnee,
    kParamMakeup,
    kParamDetector
};

//------------------------------------------------------------------------
} // namespace CV
