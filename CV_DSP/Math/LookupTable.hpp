#ifndef CVDSP_MATH_LOOKUPTABLE_HPP
#define CVDSP_MATH_LOOKUPTABLE_HPP

/**
 * @file LookupTable.hpp
 * @brief Generic linearly interpolated lookup tables for real-time DSP.
 *
 * The module provides allocation-free lookup tables for sine, cosine, and tanh
 * transfer functions. Tables are templated by scalar type, function kind, and
 * capacity so the compiler can optimize hot `lookup()` calls without virtual
 * dispatch. `prepare()` performs the expensive standard-library function calls
 * and should be executed during plug-in initialization or parameter changes,
 * never from a real-time audio callback.
 *
 * Supported capacities are 4096, 8192, 16384, and 32768 points. Lookup always
 * uses linear interpolation between adjacent samples. Sine and cosine inputs are
 * interpreted as radians and wrapped to [0, 2π). Tanh inputs are interpreted as
 * waveshaper/control values over a configurable finite domain, defaulting to
 * [-5, 5], and are clamped to the edge values outside that domain.
 *
 * Mathematical error analysis for linear interpolation:
 *
 * For a twice-differentiable function f sampled with spacing h, the absolute
 * interpolation error is bounded by
 *
 *     max(|f''(x)|) * h^2 / 8.
 *
 * Therefore, sine and cosine use max(|f''|) = 1 and h = 2π / Points. Tanh uses
 * f''(x) = -2 * sech^2(x) * tanh(x), whose global maximum absolute value is
 * 4 / (3 * sqrt(3)); with h = (max - min) / Points. Outside the finite tanh
 * domain the table clamps to tanh(min/max), so the additional saturation error
 * is bounded by max(1 - |tanh(max)|, 1 - |tanh(min)|) for the default monotonic
 * symmetric use case.
 *
 * Approximate interpolation-only error bounds:
 *
 * | Points | sine/cosine bound | tanh [-5, 5] bound |
 * | --- | --- | --- |
 * | 4096 | 2.94e-7 | 5.74e-7 |
 * | 8192 | 7.35e-8 | 1.43e-7 |
 * | 16384 | 1.84e-8 | 3.59e-8 |
 * | 32768 | 4.59e-9 | 8.96e-9 |
 *
 * The actual total error also includes floating-point rounding from table
 * generation, index arithmetic, and interpolation. For f32, rounding normally
 * dominates at the largest table sizes; for f64, the interpolation term remains
 * meaningful across the supported sizes.
 */

#include "../Core/Constants.hpp"
#include "../Core/Namespace.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace cvdsp
{
/**
 * @brief Function represented by a lookup table.
 */
enum class LookupTableFunction
{
    Sine,
    Cosine,
    Tanh
};

/**
 * @brief Named capacities supported by LookupTable.
 */
enum class LookupTableCapacity : std::size_t
{
    Points4096 = 4096,
    Points8192 = 8192,
    Points16384 = 16384,
    Points32768 = 32768
};

namespace detail
{
template <typename T>
CVDSP_FORCE_INLINE constexpr void requireLookupScalar() noexcept
{
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>,
                  "CV_DSP lookup tables require float or double scalar types.");
}

template <std::size_t Points>
CVDSP_FORCE_INLINE constexpr void requireLookupCapacity() noexcept
{
    static_assert(Points == 4096U || Points == 8192U || Points == 16384U || Points == 32768U,
                  "CV_DSP lookup table capacity must be 4096, 8192, 16384, or 32768 points.");
}

template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE T sinValue(T radians) noexcept
{
    return static_cast<T>(std::sin(radians));
}

template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE T cosValue(T radians) noexcept
{
    return static_cast<T>(std::cos(radians));
}

template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE T tanhValue(T value) noexcept
{
    return static_cast<T>(std::tanh(value));
}
} // namespace detail

/**
 * @brief Allocation-free, linearly interpolated DSP lookup table.
 *
 * @tparam T Floating-point scalar type: float or double.
 * @tparam Function Function represented by the table.
 * @tparam Points Table capacity: 4096, 8192, 16384, or 32768.
 */
template <typename T, LookupTableFunction Function, std::size_t Points>
class LookupTable
{
public:
    using Scalar = T;

    static constexpr LookupTableFunction function = Function;
    static constexpr std::size_t capacity = Points;
    static constexpr std::size_t storageSize = Points + 1U;

    /**
     * @brief Creates a table with the default tanh domain [-5, 5].
     * @note Call prepare() before lookup() to populate the table.
     */
    constexpr LookupTable() noexcept = default;

    /**
     * @brief Creates a table with a custom tanh domain.
     * @details The domain only affects Tanh tables. Sine and cosine tables keep
     *          their fixed [0, 2π) radian domain.
     */
    constexpr LookupTable(T minimumInput, T maximumInput) noexcept
        : minInput_(minimumInput)
        , maxInput_(maximumInput)
    {
    }

    /**
     * @brief Fill the table using std::sin, std::cos, or std::tanh.
     * @details This method performs all non-real-time setup work. The final
     *          guard sample duplicates the periodic endpoint for sine/cosine or
     *          stores the tanh maximum-domain value, allowing lookup() to use a
     *          branch-light interpolation path.
     */
    void prepare() noexcept
    {
        detail::requireLookupScalar<T>();
        detail::requireLookupCapacity<Points>();

        if constexpr (Function == LookupTableFunction::Sine)
        {
            preparePeriodicSine();
        }
        else if constexpr (Function == LookupTableFunction::Cosine)
        {
            preparePeriodicCosine();
        }
        else
        {
            normalizeTanhDomain();
            prepareTanh();
        }
    }

    /**
     * @brief Read the table with mandatory linear interpolation.
     *
     * Sine and cosine wrap `inputRadians` into [0, 2π). Tanh maps the input from
     * the configured [minInput(), maxInput()] domain and clamps outside it.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T lookup(T input) const noexcept
    {
        detail::requireLookupScalar<T>();
        detail::requireLookupCapacity<Points>();

        if constexpr (Function == LookupTableFunction::Sine || Function == LookupTableFunction::Cosine)
        {
            return lookupPeriodic(input);
        }
        else
        {
            return lookupClamped(input);
        }
    }

    /**
     * @brief Current minimum input for tanh tables.
     */
    CVDSP_NODISCARD constexpr T minInput() const noexcept
    {
        return minInput_;
    }

    /**
     * @brief Current maximum input for tanh tables.
     */
    CVDSP_NODISCARD constexpr T maxInput() const noexcept
    {
        return maxInput_;
    }

    /**
     * @brief Set the finite tanh input domain used by prepare() and lookup().
     * @note This does not rebuild the table automatically; call prepare() after
     *       changing the domain.
     */
    void setInputRange(T minimumInput, T maximumInput) noexcept
    {
        minInput_ = minimumInput;
        maxInput_ = maximumInput;
    }

    /**
     * @brief Direct read-only access to table storage for tests or inspection.
     */
    CVDSP_NODISCARD constexpr const std::array<T, storageSize>& data() const noexcept
    {
        return table_;
    }

    /**
     * @brief Interpolation error bound from max(|f''|) * h^2 / 8.
     */
    CVDSP_NODISCARD constexpr T interpolationErrorBound() const noexcept
    {
        if constexpr (Function == LookupTableFunction::Sine || Function == LookupTableFunction::Cosine)
        {
            constexpr T h = twoPi<T> / static_cast<T>(Points);
            return h * h * static_cast<T>(0.125);
        }
        else
        {
            // 4 / (3 * sqrt(3)) is the global maximum of |tanh''(x)|.
            constexpr T maxAbsSecondDerivative = static_cast<T>(4.0L / 5.1961524227066318805823390245176171008L);
            const T h = (maxInput_ - minInput_) / static_cast<T>(Points);
            return maxAbsSecondDerivative * h * h * static_cast<T>(0.125);
        }
    }

    /**
     * @brief Additional tanh-domain clamp error bound; zero for sine/cosine.
     */
    CVDSP_NODISCARD T saturationErrorBound() const noexcept
    {
        if constexpr (Function == LookupTableFunction::Tanh)
        {
            const T lowError = static_cast<T>(1) + detail::tanhValue(minInput_);
            const T highError = static_cast<T>(1) - detail::tanhValue(maxInput_);
            return lowError > highError ? lowError : highError;
        }
        else
        {
            return static_cast<T>(0);
        }
    }

private:
    std::array<T, storageSize> table_{};
    T minInput_ = static_cast<T>(-5);
    T maxInput_ = static_cast<T>(5);

    void preparePeriodicSine() noexcept
    {
        constexpr T step = twoPi<T> / static_cast<T>(Points);
        for (std::size_t i = 0; i < Points; ++i)
        {
            table_[i] = detail::sinValue(static_cast<T>(i) * step);
        }
        table_[Points] = table_[0];
    }

    void preparePeriodicCosine() noexcept
    {
        constexpr T step = twoPi<T> / static_cast<T>(Points);
        for (std::size_t i = 0; i < Points; ++i)
        {
            table_[i] = detail::cosValue(static_cast<T>(i) * step);
        }
        table_[Points] = table_[0];
    }

    void normalizeTanhDomain() noexcept
    {
        if (!(maxInput_ > minInput_))
        {
            minInput_ = static_cast<T>(-5);
            maxInput_ = static_cast<T>(5);
        }
    }

    void prepareTanh() noexcept
    {
        const T step = (maxInput_ - minInput_) / static_cast<T>(Points);
        for (std::size_t i = 0; i <= Points; ++i)
        {
            table_[i] = detail::tanhValue(minInput_ + static_cast<T>(i) * step);
        }
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE T lookupPeriodic(T radians) const noexcept
    {
        T normalized = radians / twoPi<T>;
        normalized -= std::floor(normalized);

        const T position = normalized * static_cast<T>(Points);
        const auto index = static_cast<std::size_t>(position);
        if (index >= Points)
        {
            return table_[0];
        }

        const T fraction = position - static_cast<T>(index);
        return table_[index] + (table_[index + 1U] - table_[index]) * fraction;
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE T lookupClamped(T input) const noexcept
    {
        if (input <= minInput_)
        {
            return table_[0];
        }
        if (input >= maxInput_)
        {
            return table_[Points];
        }

        const T position = (input - minInput_) * (static_cast<T>(Points) / (maxInput_ - minInput_));
        auto index = static_cast<std::size_t>(position);
        if (index >= Points)
        {
            index = Points - 1U;
        }

        const T fraction = position - static_cast<T>(index);
        return table_[index] + (table_[index + 1U] - table_[index]) * fraction;
    }
};

/**
 * @brief Convenience alias for callers that prefer the named capacity enum.
 */
template <typename T, LookupTableFunction Function, LookupTableCapacity Capacity>
using LookupTableWithCapacity = LookupTable<T, Function, static_cast<std::size_t>(Capacity)>;

using SineTable4096f = LookupTable<float, LookupTableFunction::Sine, 4096>;
using SineTable8192f = LookupTable<float, LookupTableFunction::Sine, 8192>;
using SineTable16384f = LookupTable<float, LookupTableFunction::Sine, 16384>;
using SineTable32768f = LookupTable<float, LookupTableFunction::Sine, 32768>;

using CosineTable4096f = LookupTable<float, LookupTableFunction::Cosine, 4096>;
using CosineTable8192f = LookupTable<float, LookupTableFunction::Cosine, 8192>;
using CosineTable16384f = LookupTable<float, LookupTableFunction::Cosine, 16384>;
using CosineTable32768f = LookupTable<float, LookupTableFunction::Cosine, 32768>;

using TanhTable4096f = LookupTable<float, LookupTableFunction::Tanh, 4096>;
using TanhTable8192f = LookupTable<float, LookupTableFunction::Tanh, 8192>;
using TanhTable16384f = LookupTable<float, LookupTableFunction::Tanh, 16384>;
using TanhTable32768f = LookupTable<float, LookupTableFunction::Tanh, 32768>;

using SineTable4096d = LookupTable<double, LookupTableFunction::Sine, 4096>;
using SineTable8192d = LookupTable<double, LookupTableFunction::Sine, 8192>;
using SineTable16384d = LookupTable<double, LookupTableFunction::Sine, 16384>;
using SineTable32768d = LookupTable<double, LookupTableFunction::Sine, 32768>;

using CosineTable4096d = LookupTable<double, LookupTableFunction::Cosine, 4096>;
using CosineTable8192d = LookupTable<double, LookupTableFunction::Cosine, 8192>;
using CosineTable16384d = LookupTable<double, LookupTableFunction::Cosine, 16384>;
using CosineTable32768d = LookupTable<double, LookupTableFunction::Cosine, 32768>;

using TanhTable4096d = LookupTable<double, LookupTableFunction::Tanh, 4096>;
using TanhTable8192d = LookupTable<double, LookupTableFunction::Tanh, 8192>;
using TanhTable16384d = LookupTable<double, LookupTableFunction::Tanh, 16384>;
using TanhTable32768d = LookupTable<double, LookupTableFunction::Tanh, 32768>;

} // namespace cvdsp

#endif // CVDSP_MATH_LOOKUPTABLE_HPP
