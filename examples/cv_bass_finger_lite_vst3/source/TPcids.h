//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kCVBassFingerLiteProcessorUID (0xc7c2c8f8, 0x75984403, 0xae8ed5c4, 0x118faeb3);
static const Steinberg::FUID kCVBassFingerLiteControllerUID (0x2ee15950, 0x388c46ff, 0x892deb15, 0xae3d5ab9);

#define CVBassFingerLiteCategory "Instrument|Bass"

enum CVBassFingerLiteParamID : Steinberg::Vst::ParamID
{
    kParamBassTone = 13200,
    kParamBassAttack,
    kParamBassVelocitySensitivity,
    kParamBassOutputGain,
    kParamBassCompression,
    kParamBassDrive,
    kParamBassLowEQ,
    kParamBassMidEQ,
    kParamBassHighEQ,
    kParamBassFingerNoise,
    kParamBassHumanize,
    kParamBassRoom
};

//------------------------------------------------------------------------
} // namespace CV
