//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kADSRVST3ProcessorUID (0x8a352a1e, 0x2cb3428f, 0xaa56a2e1, 0x502bba79);
static const Steinberg::FUID kADSRVST3ControllerUID (0xc4e0ee6c, 0xa8024fd0, 0x94f8cbda, 0x381d58ff);

#define ADSRVST3Category "Fx|Modulation"

enum ADSRVST3ParamID : Steinberg::Vst::ParamID
{
    kParamADSRGate = 9600,
    kParamADSRAttack,
    kParamADSRDecay,
    kParamADSRSustain,
    kParamADSRRelease
};

//------------------------------------------------------------------------
} // namespace CV
