#ifndef CVDSP_ADAPTERS_VST3_VST3PROCESSCONTEXTADAPTER_HPP
#define CVDSP_ADAPTERS_VST3_VST3PROCESSCONTEXTADAPTER_HPP

/**
 * @file VST3ProcessContextAdapter.hpp
 * @brief Stateless VST3 processing-context adapter for CV_DSP.
 *
 * Header-only, C++17-compatible, real-time safe adapter that converts
 * Steinberg::Vst::ProcessData and Steinberg::Vst::ProcessContext into the
 * host-neutral cvdsp::ProcessContext<T> type.
 *
 * The adapter performs no heap allocation, throws no exceptions, uses no RTTI,
 * owns no host state, and does not modify CV_DSP Core or Manager. It only
 * translates VST3 timing/transport fields into an already existing CV_DSP
 * process context.
 */

#include "../../Core/ProcessContext.hpp"

#include <cstddef>
#include <cstdint>

#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>

namespace cvdsp
{
namespace adapters
{
namespace vst3
{

/**
 * @brief Converts VST3 process timing/transport information to CV_DSP context.
 *
 * VST3ProcessContextAdapter is intentionally stateless. All API functions are
 * static and noexcept. The caller must provide the active channel count because
 * Steinberg::Vst::ProcessData::numInputs and numOutputs represent bus counts,
 * not channel counts.
 */
class VST3ProcessContextAdapter final
{
public:
    VST3ProcessContextAdapter() = delete;
    VST3ProcessContextAdapter(const VST3ProcessContextAdapter&) = delete;
    VST3ProcessContextAdapter& operator=(const VST3ProcessContextAdapter&) = delete;
    ~VST3ProcessContextAdapter() = delete;

    /**
     * @brief Converts a VST3 sample count into a CV_DSP block size.
     *
     * Negative or zero sample counts produce 0, which makes the resulting
     * cvdsp::ProcessContext<T> invalid for audio processing but still safe.
     */
    static constexpr std::size_t sampleCountToBlockSize(
        Steinberg::int32 numSamples) noexcept
    {
        return numSamples > 0
            ? static_cast<std::size_t>(numSamples)
            : static_cast<std::size_t>(0);
    }

    /**
     * @brief Returns true when a sample rate is usable by CV_DSP.
     */
    template<typename T>
    static constexpr bool isValidSampleRate(T sampleRate) noexcept
    {
        return sampleRate > static_cast<T>(0);
    }

    /**
     * @brief Returns true when a BPM value is usable by CV_DSP.
     */
    template<typename T>
    static constexpr bool isValidTempo(T tempo) noexcept
    {
        return tempo > static_cast<T>(0);
    }

    /**
     * @brief Returns true when a time signature is usable by CV_DSP.
     */
    static constexpr bool isValidTimeSignature(
        Steinberg::int32 numerator,
        Steinberg::int32 denominator) noexcept
    {
        return numerator > 0 && denominator > 0;
    }

    /**
     * @brief Returns true when a VST3 ProcessContext pointer is non-null.
     */
    static constexpr bool hasProcessContext(
        const Steinberg::Vst::ProcessContext* context) noexcept
    {
        return context != nullptr;
    }

    /**
     * @brief Returns true when a VST3 ProcessContext state flag is present.
     */
    static constexpr bool hasStateFlag(
        const Steinberg::Vst::ProcessContext& context,
        Steinberg::uint32 flag) noexcept
    {
        return (context.state & flag) != 0u;
    }

    /**
     * @brief Creates a CV_DSP context from Steinberg::Vst::ProcessData.
     *
     * @tparam T Floating-point type, normally float or double.
     * @param data VST3 process block data.
     * @param fallbackSampleRate Sample rate used when VST3 data is unavailable
     *        or invalid.
     * @param numChannels Active CV_DSP channel count for the processed path.
     * @param fallbackTempo Tempo used when VST3 tempo is unavailable or invalid.
     * @return Populated cvdsp::ProcessContext<T>.
     */
    template<typename T>
    static cvdsp::ProcessContext<T> fromProcessData(
        const Steinberg::Vst::ProcessData& data,
        T fallbackSampleRate,
        std::size_t numChannels,
        T fallbackTempo = static_cast<T>(120)) noexcept
    {
        cvdsp::ProcessContext<T> result;
        (void)fillFromProcessData(
            result,
            data,
            fallbackSampleRate,
            numChannels,
            fallbackTempo);
        return result;
    }

    /**
     * @brief Fills a CV_DSP context from Steinberg::Vst::ProcessData.
     *
     * The destination is reset first, safe defaults are applied, then valid VST3
     * transport/timing fields are copied. The return value is
     * destination.isValid().
     */
    template<typename T>
    static bool fillFromProcessData(
        cvdsp::ProcessContext<T>& destination,
        const Steinberg::Vst::ProcessData& data,
        T fallbackSampleRate,
        std::size_t numChannels,
        T fallbackTempo = static_cast<T>(120)) noexcept
    {
        return fillFromProcessContext(
            destination,
            data.processContext,
            sampleCountToBlockSize(data.numSamples),
            numChannels,
            fallbackSampleRate,
            fallbackTempo);
    }

    /**
     * @brief Creates a CV_DSP context from a VST3 ProcessContext pointer.
     *
     * A null VST3 context is valid input; in that case only safe defaults,
     * blockSize, and numChannels are used.
     */
    template<typename T>
    static cvdsp::ProcessContext<T> fromProcessContext(
        const Steinberg::Vst::ProcessContext* vstContext,
        std::size_t blockSize,
        std::size_t numChannels,
        T fallbackSampleRate,
        T fallbackTempo = static_cast<T>(120)) noexcept
    {
        cvdsp::ProcessContext<T> result;
        (void)fillFromProcessContext(
            result,
            vstContext,
            blockSize,
            numChannels,
            fallbackSampleRate,
            fallbackTempo);
        return result;
    }

    /**
     * @brief Creates a CV_DSP context from a VST3 ProcessContext reference.
     */
    template<typename T>
    static cvdsp::ProcessContext<T> fromProcessContext(
        const Steinberg::Vst::ProcessContext& vstContext,
        std::size_t blockSize,
        std::size_t numChannels,
        T fallbackSampleRate,
        T fallbackTempo = static_cast<T>(120)) noexcept
    {
        return fromProcessContext(
            &vstContext,
            blockSize,
            numChannels,
            fallbackSampleRate,
            fallbackTempo);
    }

    /**
     * @brief Fills a CV_DSP context from a VST3 ProcessContext pointer.
     *
     * The destination is reset, safe defaults are installed, then VST3 fields are
     * copied according to the VST3 validity flags. The return value is
     * destination.isValid().
     */
    template<typename T>
    static bool fillFromProcessContext(
        cvdsp::ProcessContext<T>& destination,
        const Steinberg::Vst::ProcessContext* vstContext,
        std::size_t blockSize,
        std::size_t numChannels,
        T fallbackSampleRate,
        T fallbackTempo = static_cast<T>(120)) noexcept
    {
        applyDefaults(
            destination,
            blockSize,
            numChannels,
            fallbackSampleRate,
            fallbackTempo);

        if (hasProcessContext(vstContext))
            applyVST3ProcessContext(destination, *vstContext);

        return destination.isValid();
    }

    /**
     * @brief Fills a CV_DSP context from a VST3 ProcessContext reference.
     */
    template<typename T>
    static bool fillFromProcessContext(
        cvdsp::ProcessContext<T>& destination,
        const Steinberg::Vst::ProcessContext& vstContext,
        std::size_t blockSize,
        std::size_t numChannels,
        T fallbackSampleRate,
        T fallbackTempo = static_cast<T>(120)) noexcept
    {
        return fillFromProcessContext(
            destination,
            &vstContext,
            blockSize,
            numChannels,
            fallbackSampleRate,
            fallbackTempo);
    }

private:
    /**
     * @brief Returns value when positive, otherwise fallback.
     */
    template<typename T>
    static constexpr T positiveOrDefault(T value, T fallback) noexcept
    {
        return value > static_cast<T>(0) ? value : fallback;
    }

    /**
     * @brief Applies safe CV_DSP defaults before reading optional VST3 data.
     */
    template<typename T>
    static void applyDefaults(
        cvdsp::ProcessContext<T>& destination,
        std::size_t blockSize,
        std::size_t numChannels,
        T fallbackSampleRate,
        T fallbackTempo) noexcept
    {
        destination.reset();
        destination.sampleRate = positiveOrDefault(
            fallbackSampleRate,
            static_cast<T>(44100));
        destination.blockSize = blockSize;
        destination.numChannels = numChannels;
        destination.tempo = positiveOrDefault(
            fallbackTempo,
            static_cast<T>(120));
    }

    /**
     * @brief Copies VST3 fields into the CV_DSP context when valid.
     */
    template<typename T>
    static void applyVST3ProcessContext(
        cvdsp::ProcessContext<T>& destination,
        const Steinberg::Vst::ProcessContext& source) noexcept
    {
        applySampleRate(destination, source);
        applyTransport(destination, source);
        applySamplePositionAndSeconds(destination, source);
        applyTempo(destination, source);
        applyTimeSignature(destination, source);
        applyMusicalPosition(destination, source);
    }

    /**
     * @brief Copies VST3 sample rate when positive.
     */
    template<typename T>
    static void applySampleRate(
        cvdsp::ProcessContext<T>& destination,
        const Steinberg::Vst::ProcessContext& source) noexcept
    {
        if (source.sampleRate > 0.0)
            destination.sampleRate = static_cast<T>(source.sampleRate);
    }

    /**
     * @brief Copies VST3 transport flags.
     */
    template<typename T>
    static void applyTransport(
        cvdsp::ProcessContext<T>& destination,
        const Steinberg::Vst::ProcessContext& source) noexcept
    {
        destination.isPlaying = hasStateFlag(
            source,
            Steinberg::Vst::ProcessContext::kPlaying);
        destination.isRecording = hasStateFlag(
            source,
            Steinberg::Vst::ProcessContext::kRecording);
    }

    /**
     * @brief Copies sample position and derives seconds from sample time.
     */
    template<typename T>
    static void applySamplePositionAndSeconds(
        cvdsp::ProcessContext<T>& destination,
        const Steinberg::Vst::ProcessContext& source) noexcept
    {
        if (source.projectTimeSamples >= 0)
        {
            destination.samplePosition = static_cast<std::uint64_t>(
                source.projectTimeSamples);
            destination.timeInSeconds = static_cast<T>(source.projectTimeSamples)
                / destination.sampleRate;
            return;
        }

        if (hasStateFlag(source, Steinberg::Vst::ProcessContext::kContTimeValid)
            && source.continousTimeSamples >= 0)
        {
            destination.timeInSeconds = static_cast<T>(source.continousTimeSamples)
                / destination.sampleRate;
        }
    }

    /**
     * @brief Copies VST3 BPM only when kTempoValid is present.
     */
    template<typename T>
    static void applyTempo(
        cvdsp::ProcessContext<T>& destination,
        const Steinberg::Vst::ProcessContext& source) noexcept
    {
        if (hasStateFlag(source, Steinberg::Vst::ProcessContext::kTempoValid)
            && source.tempo > 0.0)
        {
            destination.tempo = static_cast<T>(source.tempo);
        }
    }

    /**
     * @brief Copies VST3 time signature only when kTimeSigValid is present.
     */
    template<typename T>
    static void applyTimeSignature(
        cvdsp::ProcessContext<T>& destination,
        const Steinberg::Vst::ProcessContext& source) noexcept
    {
        if (hasStateFlag(source, Steinberg::Vst::ProcessContext::kTimeSigValid)
            && isValidTimeSignature(source.timeSigNumerator, source.timeSigDenominator))
        {
            destination.timeSignatureNumerator = static_cast<std::uint32_t>(
                source.timeSigNumerator);
            destination.timeSignatureDenominator = static_cast<std::uint32_t>(
                source.timeSigDenominator);
        }
    }

    /**
     * @brief Copies PPQ and bar-start PPQ according to VST3 validity flags.
     */
    template<typename T>
    static void applyMusicalPosition(
        cvdsp::ProcessContext<T>& destination,
        const Steinberg::Vst::ProcessContext& source) noexcept
    {
        if (hasStateFlag(source, Steinberg::Vst::ProcessContext::kProjectTimeMusicValid))
            destination.ppqPosition = static_cast<T>(source.projectTimeMusic);

        if (hasStateFlag(source, Steinberg::Vst::ProcessContext::kBarPositionValid))
            destination.barStartPPQ = static_cast<T>(source.barPositionMusic);
    }
};

} // namespace vst3
} // namespace adapters
} // namespace cvdsp

#endif // CVDSP_ADAPTERS_VST3_VST3PROCESSCONTEXTADAPTER_HPP
