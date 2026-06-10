//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kAmpSimulatorVST3ProcessorUID (0x8c195d74, 0x06944d7c, 0x86c7e31f, 0xc7ba90f4);
static const Steinberg::FUID kAmpSimulatorVST3ControllerUID (0x1b8ddbb9, 0x16d64e66, 0x852013b7, 0x781a3bfa);

#define AmpSimulatorVST3Category "Fx|Guitar"

enum AmpSimulatorVST3ParamID : Steinberg::Vst::ParamID
{
    kParamAmpSimulatorInputGain = 9400,
    kParamAmpSimulatorPreampDrive,
    kParamAmpSimulatorBass,
    kParamAmpSimulatorMid,
    kParamAmpSimulatorTreble,
    kParamAmpSimulatorPresence,
    kParamAmpSimulatorPowerModel,
    kParamAmpSimulatorOutputGain
};

//------------------------------------------------------------------------
} // namespace CV
