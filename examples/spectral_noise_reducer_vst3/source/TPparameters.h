//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
namespace SpectralNoiseReducerParameters {

enum : Steinberg::Vst::ParamID
{
    kBypass = 0,
    kLearnNoise = 1,
    kSubtractNoise = 2,
    kClearProfile = 3,
    kOutputGain = 4,
    kPresenceProtect = 5,
    kReductionAmount = 6,
    kSpectralFloor = 7,
    kMaxReduction = 8,
    kSmoothing = 9,
    kFrequencySmoothing = 10,
    kTransientProtection = 11,
    kMix = 12
};

inline constexpr Steinberg::Vst::ParamID kParameterIDs[] {
    kBypass,
    kLearnNoise,
    kSubtractNoise,
    kClearProfile,
    kOutputGain,
    kPresenceProtect,
    kReductionAmount,
    kSpectralFloor,
    kMaxReduction,
    kSmoothing,
    kFrequencySmoothing,
    kTransientProtection,
    kMix
};

inline constexpr Steinberg::Vst::ParamValue kDefaultValues[] {
    0.0, // Bypass
    0.0, // Perceber Ruido
    0.0, // Subtrair Ruidos
    0.0, // Limpar Perfil
    0.5, // Output Gain: -24..+24 dB
    0.5, // Presence Protect
    0.6, // Reduction Amount
    0.4444444444444444, // Spectral Floor: -120..-12 dB, default -72 dB
    0.6, // Max Reduction: 0..80 dB, default 48 dB
    0.65, // Smoothing
    0.16666666666666666, // Frequency Smoothing: 0..12 bins, default 2 bins
    0.25, // Transient Protection
    1.0 // Mix
};

inline constexpr Steinberg::int32 kParameterCount =
    static_cast<Steinberg::int32>(sizeof(kParameterIDs) / sizeof(kParameterIDs[0]));

} // namespace SpectralNoiseReducerParameters
} // namespace CV
