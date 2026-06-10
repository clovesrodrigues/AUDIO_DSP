//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kOscillatorVST3ProcessorUID (0x7dcbb68f, 0xfd7f46ad, 0xace34ed6, 0xa9fc2123);
static const Steinberg::FUID kOscillatorVST3ControllerUID (0x1f68fdc3, 0x7eeb474c, 0x8ecfe2f3, 0xf80cc8f1);

#define OscillatorVST3Category "Fx|Generator"

enum OscillatorVST3ParamID : Steinberg::Vst::ParamID
{
    kParamOscillatorWaveform = 9800,
    kParamOscillatorFrequency,
    kParamOscillatorPhase
};

//------------------------------------------------------------------------
} // namespace CV
