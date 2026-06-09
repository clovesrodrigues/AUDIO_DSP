//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace CV {
//------------------------------------------------------------------------
static const Steinberg::FUID kParametricEQVST3ProcessorUID (0x5a41446f, 0x1d764b18, 0xb9490c2d, 0x7df02a91);
static const Steinberg::FUID kParametricEQVST3ControllerUID (0x218c0f57, 0x9af04b80, 0x87f2d95a, 0x04de44fb);

#define ParametricEQVST3Category "Fx|EQ"

constexpr Steinberg::Vst::ParamID kParametricEQBandCount = 5;
constexpr Steinberg::Vst::ParamID kParametricEQParamsPerBand = 4;
constexpr Steinberg::Vst::ParamID kParametricEQBaseParamID = 1000;

enum ParametricEQBandParamOffset : Steinberg::Vst::ParamID
{
    kParametricEQTypeOffset = 0,
    kParametricEQFrequencyOffset = 1,
    kParametricEQQOffset = 2,
    kParametricEQGainOffset = 3
};

enum ParametricEQFilterType : Steinberg::int32
{
    kParametricEQLowPass = 0,
    kParametricEQHighPass,
    kParametricEQPeaking,
    kParametricEQLowShelf,
    kParametricEQHighShelf,
    kParametricEQNotch,
    kParametricEQFilterTypeCount
};

constexpr Steinberg::Vst::ParamID parametricEQParamID (Steinberg::Vst::ParamID band,
                                                       Steinberg::Vst::ParamID offset) noexcept
{
    return kParametricEQBaseParamID + band * kParametricEQParamsPerBand + offset;
}

//------------------------------------------------------------------------
} // namespace CV
