#ifndef CVDSP_MATH_INTERPOLATION_HPP
#define CVDSP_MATH_INTERPOLATION_HPP

/**
 * @file Interpolation.hpp
 * @brief Header-only interpolation algorithms for real-time audio DSP.
 *
 * Interpolation estimates a continuous value between discrete samples. In audio
 * DSP this is required whenever a read position is fractional rather than an
 * integer sample index. Common examples are variable delay lines, pitch
 * shifters, time-stretch engines, chorus, flanger, wavetable playback, and
 * sample-rate conversion. This header provides small stateless interpolators
 * that are suitable for the audio thread: no allocation, no exceptions, no RTTI,
 * and no external dependencies.
 *
 * All algorithms use the normalized fractional coordinate `fraction` in the
 * range [0, 1], where 0 means the current sample and 1 means the next sample.
 * The functions do not clamp `fraction`; callers may intentionally evaluate
 * outside [0, 1] for extrapolation, but real-time audio delay-line readers
 * should normally pass the fractional part returned by their address logic.
 *
 * Required DSP applications:
 * - Delay Lines: fractional read heads use interpolation to realize sub-sample
 *   delay times without moving memory.
 * - Pitch Shifters: modulated fractional readers resample audio at a variable
 *   ratio; interpolation controls aliasing, brightness, and modulation noise.
 * - Time Stretch: overlap-add and granular readers often need fractional grain
 *   positions for smooth playback-rate changes.
 * - Chorus: low-frequency delay modulation continuously sweeps fractional read
 *   positions; cubic-family methods reduce stepping noise compared with nearest
 *   or linear reads.
 * - Flanger: very short modulated delays are sensitive to phase error; higher
 *   order interpolation improves comb-filter stability.
 * - Sample Rate Conversion: arbitrary input/output ratios require evaluating a
 *   discrete stream at non-integer positions.
 *
 * Example: linear fractional delay read from a circular buffer. The storage is
 * owned by the object and is prepared before process(), so the audio callback
 * performs only bounded arithmetic and fixed-size array access.
 * @code{.cpp}
 * #include "CV_DSP/Math/Interpolation.hpp"
 * #include <array>
 * #include <cstddef>
 *
 * class FractionalDelayExample
 * {
 * public:
 *     void prepare(float delaySamples) noexcept
 *     {
 *         delaySamples_ = delaySamples;
 *         writeIndex_ = 0;
 *         buffer_.fill(0.0F);
 *         cvdsp::LinearInterpolation::prepare();
 *     }
 *
 *     void reset() noexcept
 *     {
 *         writeIndex_ = 0;
 *         buffer_.fill(0.0F);
 *         cvdsp::LinearInterpolation::reset();
 *     }
 *
 *     void process(float* output, const float* input, std::size_t frames) noexcept
 *     {
 *         for (std::size_t n = 0; n < frames; ++n)
 *         {
 *             buffer_[writeIndex_] = input[n];
 *
 *             const float readPosition = static_cast<float>(writeIndex_) - delaySamples_ + static_cast<float>(size);
 *             const std::size_t integerPart = static_cast<std::size_t>(readPosition);
 *             const std::size_t base = integerPart & mask;
 *             const float fraction = readPosition - static_cast<float>(integerPart);
 *             const float y0 = buffer_[base];
 *             const float y1 = buffer_[(base + 1U) & mask];
 *
 *             output[n] = cvdsp::LinearInterpolation::interpolate(y0, y1, fraction);
 *             writeIndex_ = (writeIndex_ + 1U) & mask;
 *         }
 *     }
 *
 * private:
 *     static constexpr std::size_t size = 2048U;
 *     static constexpr std::size_t mask = size - 1U;
 *     std::array<float, size> buffer_{};
 *     std::size_t writeIndex_ = 0;
 *     float delaySamples_ = 1.0F;
 * };
 * @endcode
 *
 * Example: Catmull-Rom interpolation for chorus/flanger/pitch readers.
 * @code{.cpp}
 * #include "CV_DSP/Math/Interpolation.hpp"
 * #include <array>
 * #include <cstddef>
 *
 * float readCatmullRom(const std::array<float, 4096>& buffer,
 *                      std::size_t integerIndex,
 *                      float fraction) noexcept
 * {
 *     constexpr std::size_t mask = 4096U - 1U;
 *     const float ym1 = buffer[(integerIndex - 1U) & mask];
 *     const float y0  = buffer[integerIndex & mask];
 *     const float y1  = buffer[(integerIndex + 1U) & mask];
 *     const float y2  = buffer[(integerIndex + 2U) & mask];
 *
 *     return cvdsp::CatmullRomInterpolation::interpolate(ym1, y0, y1, y2, fraction);
 * }
 * @endcode
 *
 * Example: Hermite interpolation with explicit slopes for sample-rate conversion.
 * @code{.cpp}
 * #include "CV_DSP/Math/Interpolation.hpp"
 *
 * double resampleSegment(double y0, double y1, double previous, double next, double mu) noexcept
 * {
 *     const double m0 = (y1 - previous) * 0.5;
 *     const double m1 = (next - y0) * 0.5;
 *     return cvdsp::HermiteInterpolation::interpolate(y0, y1, m0, m1, mu);
 * }
 * @endcode
 */

#include "../Core/Namespace.hpp"

#include <type_traits>

namespace cvdsp
{
namespace detail
{
template <typename T>
CVDSP_FORCE_INLINE constexpr void requireInterpolationScalar() noexcept
{
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>,
                  "CV_DSP interpolation requires float or double scalar types.");
}
} // namespace detail

/**
 * @brief First-order interpolation between two adjacent samples.
 *
 * Mathematical theory:
 * Linear interpolation fits a first-degree polynomial through the points
 * `(0, y0)` and `(1, y1)`. The basis functions are `(1 - t)` and `t`, so the
 * estimated value is `y(t) = y0 * (1 - t) + y1 * t`, equivalently
 * `y0 + t * (y1 - y0)`. It is continuous in amplitude but its derivative is
 * discontinuous at sample boundaries.
 *
 * DSP applications:
 * It is commonly used in delay lines, pitch shifters, time stretchers, chorus,
 * flangers, and low/medium quality sample-rate conversion when minimum latency
 * and CPU cost are more important than high-frequency accuracy. In a modulated
 * delay, it prevents hard sample jumps but can introduce mild high-frequency
 * attenuation and modulation sidebands.
 *
 * Computational cost:
 * 1 subtraction, 1 multiplication, and 1 addition. This is the cheapest method
 * in this file and is highly compiler-friendly for scalar and auto-vectorized
 * processing.
 */
struct LinearInterpolation
{
    /** @brief Stateless setup hook supplied for a uniform CV_DSP module API. */
    CVDSP_FORCE_INLINE static constexpr void prepare() noexcept {}

    /** @brief Stateless reset hook supplied for a uniform CV_DSP module API. */
    CVDSP_FORCE_INLINE static constexpr void reset() noexcept {}

    /**
     * @brief Interpolate between `y0` at t=0 and `y1` at t=1.
     * @tparam T Floating-point scalar type: float or double.
     * @param y0 Sample at the lower integer position.
     * @param y1 Sample at the next integer position.
     * @param fraction Normalized fractional position.
     * @return Interpolated value.
     */
    template <typename T>
    CVDSP_NODISCARD CVDSP_FORCE_INLINE static constexpr T interpolate(T y0, T y1, T fraction) noexcept
    {
        detail::requireInterpolationScalar<T>();
        return y0 + fraction * (y1 - y0);
    }
};

/**
 * @brief Four-point cubic interpolation optimized for fractional audio reads.
 *
 * Mathematical theory:
 * This interpolator constructs a third-degree polynomial over the segment from
 * `y1` at t=0 to `y2` at t=1 using the neighboring samples `y0` and `y3` to
 * shape curvature. In Horner form the polynomial is
 * `(((a0 * t) + a1) * t + a2) * t + a3`, with
 * `a0 = y3 - y2 - y0 + y1`, `a1 = y0 - y1 - a0`, `a2 = y2 - y0`, and
 * `a3 = y1`. The polynomial exactly reaches `y1` and `y2`, while the outer
 * samples influence the slope and curvature. This classic audio cubic is not
 * identical to Catmull-Rom or Lagrange; it trades a little mathematical
 * exactness for a compact coefficient set.
 *
 * DSP applications:
 * Useful for fractional delay lines, pitch shifters, time stretch engines,
 * chorus, flanger, and sample-rate conversion where linear interpolation is too
 * grainy but a slightly more expensive four-sample read is acceptable.
 *
 * Computational cost:
 * Coefficient construction uses several additions/subtractions. Evaluation uses
 * 3 multiplications and 3 additions in Horner form. No divisions are required.
 */
struct CubicInterpolation
{
    /** @brief Stateless setup hook supplied for a uniform CV_DSP module API. */
    CVDSP_FORCE_INLINE static constexpr void prepare() noexcept {}

    /** @brief Stateless reset hook supplied for a uniform CV_DSP module API. */
    CVDSP_FORCE_INLINE static constexpr void reset() noexcept {}

    /**
     * @brief Interpolate between center samples `y1` and `y2` using four points.
     * @tparam T Floating-point scalar type: float or double.
     * @param y0 Sample before the interpolation segment.
     * @param y1 Segment start sample at t=0.
     * @param y2 Segment end sample at t=1.
     * @param y3 Sample after the interpolation segment.
     * @param fraction Normalized fractional position.
     * @return Interpolated value.
     */
    template <typename T>
    CVDSP_NODISCARD CVDSP_FORCE_INLINE static constexpr T interpolate(T y0, T y1, T y2, T y3, T fraction) noexcept
    {
        detail::requireInterpolationScalar<T>();
        const T a0 = y3 - y2 - y0 + y1;
        const T a1 = y0 - y1 - a0;
        const T a2 = y2 - y0;
        const T a3 = y1;
        return ((a0 * fraction + a1) * fraction + a2) * fraction + a3;
    }
};

/**
 * @brief Cubic Hermite interpolation with explicit endpoint tangents.
 *
 * Mathematical theory:
 * Hermite interpolation fits a cubic polynomial to two endpoint values and two
 * endpoint derivatives. For t in [0, 1], the basis functions are
 * `h00 = 2t^3 - 3t^2 + 1`, `h10 = t^3 - 2t^2 + t`,
 * `h01 = -2t^3 + 3t^2`, and `h11 = t^3 - t^2`. The result is
 * `h00*y0 + h10*m0 + h01*y1 + h11*m1`, where `m0` and `m1` are slopes measured
 * in sample-value units per normalized segment. Explicit tangents let the caller
 * control smoothness, monotonicity, and transient behavior.
 *
 * DSP applications:
 * Hermite interpolation is useful when a resampler, time-stretch reader, pitch
 * shifter, delay line, chorus, or flanger already has reliable derivative
 * estimates. It can preserve smoother motion than linear interpolation and can
 * be tuned to avoid excessive overshoot in sensitive feedback structures.
 *
 * Computational cost:
 * Builds t^2 and t^3, evaluates four basis functions, then combines four terms.
 * It is more expensive than linear interpolation but remains small and branch
 * free. When Catmull-Rom style tangents are desired, use
 * CatmullRomInterpolation to avoid repeated caller-side tangent code.
 */
struct HermiteInterpolation
{
    /** @brief Stateless setup hook supplied for a uniform CV_DSP module API. */
    CVDSP_FORCE_INLINE static constexpr void prepare() noexcept {}

    /** @brief Stateless reset hook supplied for a uniform CV_DSP module API. */
    CVDSP_FORCE_INLINE static constexpr void reset() noexcept {}

    /**
     * @brief Interpolate between two samples using explicit tangents.
     * @tparam T Floating-point scalar type: float or double.
     * @param y0 Segment start sample at t=0.
     * @param y1 Segment end sample at t=1.
     * @param m0 Tangent at y0 in value units per normalized segment.
     * @param m1 Tangent at y1 in value units per normalized segment.
     * @param fraction Normalized fractional position.
     * @return Interpolated value.
     */
    template <typename T>
    CVDSP_NODISCARD CVDSP_FORCE_INLINE static constexpr T interpolate(T y0, T y1, T m0, T m1, T fraction) noexcept
    {
        detail::requireInterpolationScalar<T>();
        const T t2 = fraction * fraction;
        const T t3 = t2 * fraction;
        const T h00 = static_cast<T>(2) * t3 - static_cast<T>(3) * t2 + static_cast<T>(1);
        const T h10 = t3 - static_cast<T>(2) * t2 + fraction;
        const T h01 = static_cast<T>(-2) * t3 + static_cast<T>(3) * t2;
        const T h11 = t3 - t2;
        return h00 * y0 + h10 * m0 + h01 * y1 + h11 * m1;
    }
};

/**
 * @brief Catmull-Rom spline interpolation through the two center samples.
 *
 * Mathematical theory:
 * Catmull-Rom is a cubic Hermite spline whose tangents are estimated from
 * neighboring samples: `m0 = 0.5 * (y1 - ym1)` and `m1 = 0.5 * (y2 - y0)`.
 * With samples `ym1, y0, y1, y2`, it interpolates the segment from `y0` at t=0
 * to `y1` at t=1 and passes exactly through the center points. The 0.5 factor
 * gives the standard uniform Catmull-Rom spline, a good default for evenly
 * spaced audio samples.
 *
 * DSP applications:
 * This is a strong general-purpose choice for modulated delay lines, pitch
 * shifters, time stretch readers, chorus, flanger, and sample-rate conversion.
 * It usually sounds brighter and smoother than linear interpolation while
 * requiring only four neighboring samples and no state.
 *
 * Computational cost:
 * Tangent estimation uses two subtractions and two multiplications by 0.5, then
 * a cubic Hermite evaluation. The optimized Horner form below reduces the final
 * polynomial to 3 multiplications after coefficient construction.
 */
struct CatmullRomInterpolation
{
    /** @brief Stateless setup hook supplied for a uniform CV_DSP module API. */
    CVDSP_FORCE_INLINE static constexpr void prepare() noexcept {}

    /** @brief Stateless reset hook supplied for a uniform CV_DSP module API. */
    CVDSP_FORCE_INLINE static constexpr void reset() noexcept {}

    /**
     * @brief Interpolate between `y0` and `y1` using their immediate neighbors.
     * @tparam T Floating-point scalar type: float or double.
     * @param ym1 Sample before y0.
     * @param y0 Segment start sample at t=0.
     * @param y1 Segment end sample at t=1.
     * @param y2 Sample after y1.
     * @param fraction Normalized fractional position.
     * @return Interpolated value.
     */
    template <typename T>
    CVDSP_NODISCARD CVDSP_FORCE_INLINE static constexpr T interpolate(T ym1, T y0, T y1, T y2, T fraction) noexcept
    {
        detail::requireInterpolationScalar<T>();
        const T half = static_cast<T>(0.5);
        const T a0 = half * (static_cast<T>(2) * y0);
        const T a1 = half * (-ym1 + y1);
        const T a2 = half * (static_cast<T>(2) * ym1 - static_cast<T>(5) * y0 + static_cast<T>(4) * y1 - y2);
        const T a3 = half * (-ym1 + static_cast<T>(3) * y0 - static_cast<T>(3) * y1 + y2);
        return ((a3 * fraction + a2) * fraction + a1) * fraction + a0;
    }
};

/**
 * @brief Four-point third-order Lagrange interpolation.
 *
 * Mathematical theory:
 * Lagrange interpolation constructs the unique cubic polynomial that passes
 * exactly through four uniformly spaced samples. This implementation uses the
 * support points x=-1, 0, 1, and 2 with values `ym1, y0, y1, y2`, then evaluates
 * the polynomial for t in [0, 1]. The basis functions are products of distance
 * ratios from all other sample positions, so each input sample contributes a
 * weight that becomes 1 at its own grid position and 0 at the others.
 *
 * DSP applications:
 * Lagrange interpolation is widely used for fractional delay lines, pitch
 * shifters, time stretchers, chorus, flangers, and sample-rate conversion when
 * exact passage through the four local samples and good passband behavior are
 * desired. It can overshoot on sharp transients, so feedback delay networks and
 * nonlinear processors should be gain-staged carefully.
 *
 * Computational cost:
 * The direct basis form uses several multiplications and constant divisions.
 * Constant denominators are represented as compile-time multipliers, avoiding
 * runtime division. It is typically more expensive than the compact cubic form
 * but provides a well-defined polynomial interpolant through all four points.
 */
struct LagrangeInterpolation
{
    /** @brief Stateless setup hook supplied for a uniform CV_DSP module API. */
    CVDSP_FORCE_INLINE static constexpr void prepare() noexcept {}

    /** @brief Stateless reset hook supplied for a uniform CV_DSP module API. */
    CVDSP_FORCE_INLINE static constexpr void reset() noexcept {}

    /**
     * @brief Interpolate between `y0` and `y1` with a four-point Lagrange polynomial.
     * @tparam T Floating-point scalar type: float or double.
     * @param ym1 Sample at x=-1.
     * @param y0 Segment start sample at x=0.
     * @param y1 Segment end sample at x=1.
     * @param y2 Sample at x=2.
     * @param fraction Normalized fractional position.
     * @return Interpolated value.
     */
    template <typename T>
    CVDSP_NODISCARD CVDSP_FORCE_INLINE static constexpr T interpolate(T ym1, T y0, T y1, T y2, T fraction) noexcept
    {
        detail::requireInterpolationScalar<T>();
        const T t = fraction;
        const T l0 = -((t) * (t - static_cast<T>(1)) * (t - static_cast<T>(2))) * static_cast<T>(1.0 / 6.0);
        const T l1 = (t + static_cast<T>(1)) * (t - static_cast<T>(1)) * (t - static_cast<T>(2)) * static_cast<T>(0.5);
        const T l2 = -((t + static_cast<T>(1)) * t * (t - static_cast<T>(2))) * static_cast<T>(0.5);
        const T l3 = (t + static_cast<T>(1)) * t * (t - static_cast<T>(1)) * static_cast<T>(1.0 / 6.0);
        return ym1 * l0 + y0 * l1 + y1 * l2 + y2 * l3;
    }
};
} // namespace cvdsp

#endif // CVDSP_MATH_INTERPOLATION_HPP
