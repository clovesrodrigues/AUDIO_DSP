//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kRoomReverbVST3ProcessorUID (0x92A31D71, 0x0B384595, 0xA51B6A2D, 0x114E7201);
static const Steinberg::FUID kRoomReverbVST3ControllerUID (0x4552A871, 0xC99A4C8A, 0x850DB67E, 0x4F68C101);

#define RoomReverbVST3Category "Fx|Reverb"

enum RoomReverbVST3ParamID : Steinberg::Vst::ParamID
{
    kParamMix = 9100,
    kParamDecay,
    kParamDamping,
    kParamPreDelay
};

//------------------------------------------------------------------------
} // namespace CV
