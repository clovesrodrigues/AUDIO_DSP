#ifndef CVDSP_GUITAR_PEDALS_PEDALTYPES_HPP
#define CVDSP_GUITAR_PEDALS_PEDALTYPES_HPP

/**
 * @file PedalTypes.hpp
 * @brief Common enums and numeric limits for CV_DSP guitar pedals.
 *
 * Header-only, C++20 and real-time safe. This file contains no processing code
 * and performs no allocation.
 */

#include <type_traits>

#include "../../Core/Types.hpp"

namespace cvdsp::guitar::pedals
{

/**
 * @brief CPU/quality policy used by pedal stages that can choose approximations.
 */
enum class PedalQualityMode : u32
{
    Eco = 0,   ///< Prefer lower CPU approximations.
    Normal,    ///< Balanced default for real-time playing.
    Studio     ///< Prefer higher quality where available.
};

/**
 * @brief Oversampling policy for nonlinear stages.
 */
enum class PedalOversamplingMode : u32
{
    Off = 0,
    x2,
    x4,
    x8
};

/**
 * @brief Transfer-curve family used by parametrized pedal clippers.
 */
enum class PedalClipMode : u32
{
    Hard = 0,
    Soft,
    Tanh,
    Arctan,
    Cubic,
    Foldback,
    Hybrid
};

/**
 * @brief Musical voicing hint shared by pre/post tone stages.
 */
enum class PedalToneMode : u32
{
    Flat = 0,
    Dark,
    Neutral,
    Bright,
    Scooped,
    MidForward
};

/**
 * @brief Bypass behavior requested by hosts or pedal wrappers.
 */
enum class PedalBypassMode : u32
{
    Off = 0,      ///< Processing is active.
    HardBypass,   ///< Direct input-to-output bypass.
    SoftBypass    ///< Reserved for smoothed/crossfaded bypass.
};

/**
 * @brief Optional rectification stage for octave/fuzz coloration.
 */
enum class PedalRectifyMode : u32
{
    Off = 0,  ///< Leave the waveform polarity unchanged.
    Half,     ///< Remove the negative half-cycle.
    Full      ///< Full-wave rectify for octave-fuzz coloration.
};

/**
 * @brief Macro voicings for chainsaw/high-gain distortion pedals.
 */
enum class ChainsawVoiceMode : u32
{
    ClassicSwedish = 0, ///< Loose low end and strong dual-mid chainsaw resonance.
    ModernTight,        ///< Tighter low cut and more controlled high end.
    DoomLoose,          ///< Bigger low mids and looser low cut.
    DeathMetalScoop     ///< More scoop with aggressive high-mid grind.
};

/**
 * @brief Shared scalar constants for guitar pedal ranges.
 */
template<typename T>
struct PedalConstants
{
    static_assert(std::is_floating_point_v<T>, "PedalConstants requires a floating point type");

    static constexpr T kDefaultSampleRate = static_cast<T>(44100);
    static constexpr T kDefaultRampTimeSeconds = static_cast<T>(0.005);

    static constexpr T kMinFrequencyHz = static_cast<T>(5);
    static constexpr T kMaxFrequencyHz = static_cast<T>(22000);
    static constexpr T kMinGuitarFrequencyHz = static_cast<T>(20);
    static constexpr T kMaxGuitarFrequencyHz = static_cast<T>(12000);

    static constexpr T kMinQ = static_cast<T>(0.1);
    static constexpr T kMaxQ = static_cast<T>(24);
    static constexpr T kDefaultQ = static_cast<T>(0.7071067811865475244);

    static constexpr T kMinGainDb = static_cast<T>(-60);
    static constexpr T kMaxGainDb = static_cast<T>(36);
    static constexpr T kMinInputGainDb = static_cast<T>(-24);
    static constexpr T kMaxInputGainDb = static_cast<T>(24);
    static constexpr T kMinOutputGainDb = static_cast<T>(-60);
    static constexpr T kMaxOutputGainDb = static_cast<T>(12);
    static constexpr T kMinDriveDb = static_cast<T>(0);
    static constexpr T kMaxDriveDb = static_cast<T>(60);

    static constexpr T kMinClipThreshold = static_cast<T>(0.01);
    static constexpr T kMaxClipThreshold = static_cast<T>(4);
    static constexpr T kDefaultClipThreshold = static_cast<T>(1);
};

using PedalConstantsF = PedalConstants<f32>;
using PedalConstantsD = PedalConstants<f64>;

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_PEDALTYPES_HPP
