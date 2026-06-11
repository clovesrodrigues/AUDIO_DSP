//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
namespace SpectralNoiseReducerParameters {

enum : Steinberg::Vst::ParamID
{
    kLearnNoise = 0,
    kSubtractNoise = 1,
    kOutputGain = 2,
    kPresenceProtect = 3,
    kSmoothing = 4
};

inline constexpr Steinberg::Vst::ParamID kParameterIDs[] {
    kLearnNoise,
    kSubtractNoise,
    kOutputGain,
    kPresenceProtect,
    kSmoothing
};

inline constexpr Steinberg::Vst::ParamValue kDefaultValues[] {
    0.0, // Perceber Ruido
    0.0, // Subtrair Ruido
    0.5, // Ganho: -24..+24 dB
    0.5, // Presenca
    0.65 // Smooth
};

inline constexpr Steinberg::int32 kParameterCount =
    static_cast<Steinberg::int32>(sizeof(kParameterIDs) / sizeof(kParameterIDs[0]));

} // namespace SpectralNoiseReducerParameters
} // namespace CV
