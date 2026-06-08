#ifndef CVDSP_GUITAR_AMPSIMULATOR_HPP
#define CVDSP_GUITAR_AMPSIMULATOR_HPP

/**
 * @file AmpSimulator.hpp
 * @brief Complete real-time safe guitar amplifier simulator.
 *
 * Header-Only.
 * C++20.
 * No external dependencies.
 * No allocations inside process().
 * No exceptions thrown by audio processing.
 *
 * Mandatory dependencies:
 * - TubePreamp.hpp
 * - ToneStack.hpp
 * - PowerAmp.hpp
 * - CabinetSimulator.hpp
 *
 * Optional dependency:
 * - Dynamics/NoiseGate.hpp
 *
 * @section signalflow Signal flow
 *
 * @verbatim
 *   Input
 *     -> NoiseGate
 *     -> TubePreamp
 *     -> ToneStack
 *     -> PowerAmp
 *     -> CabinetSimulator
 *     -> Output
 * @endverbatim
 *
 * The simulator is intentionally modular: every stage is an independent CV_DSP
 * processor with its own prepare(), reset(), and process() method. The class
 * only coordinates gain staging, ordering, and bypass decisions. No stage is
 * reimplemented here.
 *
 * @section dspmath DSP model
 *
 * The input stage applies a linear gain @f$ g_i @f$ before nonlinear tube
 * processing:
 *
 * @f[
 *     x_0[n] = g_i x[n]
 * @f]
 *
 * The optional gate estimates input level and multiplies the signal by a
 * smoothed gain @f$ g_g[n] @f$ so pickup noise is reduced before distortion:
 *
 * @f[
 *     x_1[n] = g_g[n] x_0[n]
 * @f]
 *
 * The preamp then performs cascaded triode saturation. Placing it before the
 * tone stack follows common guitar-amplifier topology: nonlinear harmonic
 * generation happens first, and the tone network shapes the generated spectrum.
 *
 * The tone stack is a concrete ToneStack-compatible processor supplied by the
 * template parameter. By default it is MarshallToneStack, but Fender, Vox, Mesa,
 * or user-defined compatible tone stacks can be selected without changing this
 * file.
 *
 * The power amplifier stage models push-pull pentode coloration, supply sag,
 * and level-dependent compression. Finally, the cabinet simulator applies a
 * cabinet impulse response if one has been loaded; otherwise it is transparent.
 *
 * The output stage applies a final linear gain @f$ g_o @f$:
 *
 * @f[
 *     y[n] = g_o x_5[n]
 * @f]
 *
 * @tparam T Floating-point sample type. Supported types are float and double.
 * @tparam ToneStackProcessor Concrete tone-stack processor type. It must expose
 *         prepare(T), reset(), process(T), setBass(T), setMid(T), setTreble(T),
 *         and setPresence(T).
 * @tparam CabinetFFTSize FFT size used by CabinetSimulator.
 * @tparam CabinetMaxIRSamples Maximum cabinet impulse-response length.
 */

#include <cstddef>
#include <string>
#include <type_traits>

#include "TubePreamp.hpp"
#include "ToneStack.hpp"
#include "MarshallToneStack.hpp"
#include "PowerAmp.hpp"
#include "CabinetSimulator.hpp"
#include "../Dynamics/NoiseGate.hpp"

namespace cvdsp
{

template<
    typename T,
    typename ToneStackProcessor = MarshallToneStack<T>,
    std::size_t CabinetFFTSize = 2048,
    std::size_t CabinetMaxIRSamples = 65536>
class AmpSimulator final
{
    static_assert(
        std::is_floating_point_v<T>,
        "AmpSimulator requires a floating point type");

public:

    using value_type = T;
    using ToneStackType = ToneStackProcessor;
    using CabinetType =
        CabinetSimulator<
            T,
            CabinetFFTSize,
            CabinetMaxIRSamples>;
    using NoiseGateType = dynamics::NoiseGate<T>;
    using PowerAmpType = PowerAmp<T>;
    using TubePreampType = TubePreamp<T>;

public:

    /**
     * @brief Constructs an amplifier simulator with deterministic defaults.
     */
    constexpr AmpSimulator() noexcept = default;

    /**
     * @brief Prepares every internal DSP stage.
     *
     * Call this outside the audio callback before process(). The method may
     * initialize cabinet-convolution memory through CabinetSimulator::prepare(),
     * therefore it is intentionally not part of the audio-rate path.
     *
     * @param sampleRate Host sample rate in Hz. Non-positive values fall back
     *        to 44100 Hz for numerical safety.
     * @return true when the cabinet convolution engine is prepared.
     */
    [[nodiscard]] bool prepare(
        T sampleRate)
        noexcept
    {
        sampleRate_ =
            (sampleRate > static_cast<T>(0))
                ? sampleRate
                : static_cast<T>(44100);

        noiseGate_.prepare(
            sampleRate_);

        preamp_.prepare(
            sampleRate_);

        toneStack_.prepare(
            sampleRate_);

        powerAmp_.prepare(
            sampleRate_);

        const bool cabinetPrepared =
            cabinet_.prepare(
                sampleRate_);

        reset();

        return cabinetPrepared;
    }

    /**
     * @brief Resets all stateful stages without changing parameters.
     *
     * Real-time safe if called by the host during a stopped or suspended audio
     * stream. It performs no dynamic allocation and throws no exceptions.
     */
    void reset()
        noexcept
    {
        noiseGate_.reset();
        preamp_.reset();
        toneStack_.reset();
        powerAmp_.reset();
        cabinet_.reset();
    }

    /**
     * @brief Processes one mono guitar sample through the full amplifier chain.
     *
     * Processing order:
     * 1. Input gain.
     * 2. Optional noise gate.
     * 3. Tube preamp.
     * 4. Tone stack.
     * 5. Power amplifier.
     * 6. Optional cabinet simulator.
     * 7. Output gain.
     *
     * @param input Input sample.
     * @return Amplified output sample.
     *
     * Real-time safe: O(1) plus the selected cabinet engine work, noexcept,
     * no allocation, no file I/O, and no parameter lookup by name.
     */
    [[nodiscard]] T process(
        T input)
        noexcept
    {
        T x =
            input
            *
            inputGain_;

        if (noiseGateEnabled_)
        {
            x =
                noiseGate_.process(
                    x);
        }

        x =
            preamp_.process(
                x);

        x =
            toneStack_.process(
                x);

        x =
            powerAmp_.process(
                x);

        if (cabinetEnabled_)
        {
            x =
                cabinet_.process(
                    x);
        }

        return
            x
            *
            outputGain_;
    }

    /**
     * @brief Enables or disables the input noise gate stage.
     */
    void setNoiseGateEnabled(
        bool enabled)
        noexcept
    {
        noiseGateEnabled_ = enabled;
    }

    /**
     * @brief Enables or disables cabinet processing.
     */
    void setCabinetEnabled(
        bool enabled)
        noexcept
    {
        cabinetEnabled_ = enabled;
    }

    /**
     * @brief Sets linear gain before the first processing stage.
     */
    void setInputGain(
        T gain)
        noexcept
    {
        inputGain_ = gain;
    }

    /**
     * @brief Sets linear gain after the final processing stage.
     */
    void setOutputGain(
        T gain)
        noexcept
    {
        outputGain_ = gain;
    }

    /**
     * @brief Convenience setter for preamp drive.
     */
    void setPreampDrive(
        T drive)
        noexcept
    {
        preamp_.setDrive(
            drive);
    }

    /**
     * @brief Convenience setter for preamp stage count.
     */
    void setPreampStages(
        std::size_t stages)
        noexcept
    {
        preamp_.setNumStages(
            stages);
    }

    /**
     * @brief Convenience setter for tone-stack bass control, normalized 0..1.
     */
    void setBass(
        T value)
        noexcept
    {
        toneStack_.setBass(
            clamp01(value));
    }

    /**
     * @brief Convenience setter for tone-stack mid control, normalized 0..1.
     */
    void setMid(
        T value)
        noexcept
    {
        toneStack_.setMid(
            clamp01(value));
    }

    /**
     * @brief Convenience setter for tone-stack treble control, normalized 0..1.
     */
    void setTreble(
        T value)
        noexcept
    {
        toneStack_.setTreble(
            clamp01(value));
    }

    /**
     * @brief Convenience setter for tone-stack presence control, normalized 0..1.
     */
    void setPresence(
        T value)
        noexcept
    {
        toneStack_.setPresence(
            clamp01(value));
    }

    /**
     * @brief Selects the power-amp tube model.
     */
    void setPowerAmpModel(
        typename PowerAmp<T>::Model model)
        noexcept
    {
        powerAmp_.setModel(
            model);
    }

    /**
     * @brief Loads a cabinet impulse response outside the audio callback.
     *
     * This method may perform file I/O and should never be called from
     * process(). The real-time process path only uses the prepared convolution
     * state.
     */
    [[nodiscard]] bool loadCabinet(
        const std::string& filePath,
        bool normalize = true,
        bool trimSilence = true)
        noexcept
    {
        return cabinet_.loadCabinet(
            filePath,
            normalize,
            trimSilence);
    }

    /**
     * @brief Unloads the current cabinet outside the audio callback.
     */
    void unloadCabinet()
        noexcept
    {
        cabinet_.unloadCabinet();
    }

    [[nodiscard]] constexpr T getSampleRate() const noexcept
    {
        return sampleRate_;
    }

    [[nodiscard]] constexpr T getInputGain() const noexcept
    {
        return inputGain_;
    }

    [[nodiscard]] constexpr T getOutputGain() const noexcept
    {
        return outputGain_;
    }

    [[nodiscard]] constexpr bool isNoiseGateEnabled() const noexcept
    {
        return noiseGateEnabled_;
    }

    [[nodiscard]] constexpr bool isCabinetEnabled() const noexcept
    {
        return cabinetEnabled_;
    }

    [[nodiscard]] bool isCabinetLoaded() const noexcept
    {
        return cabinet_.isLoaded();
    }

    [[nodiscard]] NoiseGateType& getNoiseGate() noexcept
    {
        return noiseGate_;
    }

    [[nodiscard]] const NoiseGateType& getNoiseGate() const noexcept
    {
        return noiseGate_;
    }

    [[nodiscard]] TubePreampType& getPreamp() noexcept
    {
        return preamp_;
    }

    [[nodiscard]] const TubePreampType& getPreamp() const noexcept
    {
        return preamp_;
    }

    [[nodiscard]] ToneStackType& getToneStack() noexcept
    {
        return toneStack_;
    }

    [[nodiscard]] const ToneStackType& getToneStack() const noexcept
    {
        return toneStack_;
    }

    [[nodiscard]] PowerAmpType& getPowerAmp() noexcept
    {
        return powerAmp_;
    }

    [[nodiscard]] const PowerAmpType& getPowerAmp() const noexcept
    {
        return powerAmp_;
    }

    [[nodiscard]] CabinetType& getCabinet() noexcept
    {
        return cabinet_;
    }

    [[nodiscard]] const CabinetType& getCabinet() const noexcept
    {
        return cabinet_;
    }

private:

    [[nodiscard]] static constexpr T clamp01(
        T value)
        noexcept
    {
        return
            (value < static_cast<T>(0))
                ? static_cast<T>(0)
                :
                (
                    (value > static_cast<T>(1))
                        ? static_cast<T>(1)
                        : value
                );
    }

private:

    T sampleRate_ =
        static_cast<T>(44100);

    T inputGain_ =
        static_cast<T>(1);

    T outputGain_ =
        static_cast<T>(1);

    bool noiseGateEnabled_ =
        true;

    bool cabinetEnabled_ =
        true;

    NoiseGateType
        noiseGate_;

    TubePreampType
        preamp_;

    ToneStackType
        toneStack_;

    PowerAmpType
        powerAmp_;

    CabinetType
        cabinet_;
};

using AmpSimulatorF =
    AmpSimulator<float>;

using AmpSimulatorD =
    AmpSimulator<double>;

} // namespace cvdsp

#endif // CVDSP_GUITAR_AMPSIMULATOR_HPP
