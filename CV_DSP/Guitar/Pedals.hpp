#ifndef CVDSP_GUITAR_PEDALS_HPP
#define CVDSP_GUITAR_PEDALS_HPP

/**
 * @file Pedals.hpp
 * @brief Aggregate include for CV_DSP guitar distortion pedals.
 *
 * This header intentionally contains no processing logic. It only exposes the
 * shared pedal infrastructure and the concrete pedal DSP classes.
 */

#include "Pedals/PedalTypes.hpp"
#include "Pedals/PedalParameterIDs.hpp"
#include "Pedals/PedalParameterUtils.hpp"
#include "Pedals/PedalGainStage.hpp"
#include "Pedals/PedalMix.hpp"
#include "Pedals/PedalClipper.hpp"
#include "Pedals/PedalPreFilter.hpp"
#include "Pedals/PedalPostFilter.hpp"
#include "Pedals/PedalDriveCore.hpp"
#include "Pedals/SustainerDSP.hpp"
#include "Pedals/WahWahDSP.hpp"
#include "Pedals/PhaserDSP.hpp"
#include "Pedals/ClassicOverdriveDSP.hpp"
#include "Pedals/VintageHardDistortionDSP.hpp"
#include "Pedals/VintageFuzzDSP.hpp"
#include "Pedals/ChainsawMetalDSP.hpp"

#endif // CVDSP_GUITAR_PEDALS_HPP
