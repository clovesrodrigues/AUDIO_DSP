#ifndef CVDSP_EQ_EQUALIZER_HPP
#define CVDSP_EQ_EQUALIZER_HPP

/**
 * @file Equalizer.hpp
 * @brief Umbrella header for CV_DSP equalization modules.
 *
 * Includes the fixed-capacity parametric and graphic EQ implementations and
 * declares convenient aliases for common mono processing layouts. The module is
 * header-only, C++20-compatible, and independent of host SDKs.
 */

#include "ParametricEQ.hpp"
#include "GraphicEQ.hpp"

namespace cvdsp::eq
{

using ParametricEQ3F = ParametricEQ<cvdsp::f32, 3>;
using ParametricEQ5F = ParametricEQ<cvdsp::f32, 5>;
using ParametricEQ8F = ParametricEQ<cvdsp::f32, 8>;
using ParametricEQ3D = ParametricEQ<cvdsp::f64, 3>;
using ParametricEQ5D = ParametricEQ<cvdsp::f64, 5>;
using ParametricEQ8D = ParametricEQ<cvdsp::f64, 8>;

using GraphicEQ10F = GraphicEQ<cvdsp::f32, 10>;
using GraphicEQ15F = GraphicEQ<cvdsp::f32, 15>;
using GraphicEQ31F = GraphicEQ<cvdsp::f32, 31>;
using GraphicEQ10D = GraphicEQ<cvdsp::f64, 10>;
using GraphicEQ15D = GraphicEQ<cvdsp::f64, 15>;
using GraphicEQ31D = GraphicEQ<cvdsp::f64, 31>;

} // namespace cvdsp::eq

namespace cvdsp
{
using eq::GraphicEQ10D;
using eq::GraphicEQ10F;
using eq::GraphicEQ15D;
using eq::GraphicEQ15F;
using eq::GraphicEQ31D;
using eq::GraphicEQ31F;
using eq::ParametricEQ3D;
using eq::ParametricEQ3F;
using eq::ParametricEQ5D;
using eq::ParametricEQ5F;
using eq::ParametricEQ8D;
using eq::ParametricEQ8F;
} // namespace cvdsp

#endif // CVDSP_EQ_EQUALIZER_HPP
