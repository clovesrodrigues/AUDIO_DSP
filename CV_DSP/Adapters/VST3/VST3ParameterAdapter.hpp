#ifndef CVDSP_ADAPTERS_VST3_VST3PARAMETERADAPTER_HPP
#define CVDSP_ADAPTERS_VST3_VST3PARAMETERADAPTER_HPP

/**
 * @file VST3ParameterAdapter.hpp
 * @brief Stateless VST3 parameter automation adapter for CV_DSP.
 *
 * Header-only, C++17-compatible, real-time safe adapter that translates
 * Steinberg::Vst::IParameterChanges and Steinberg::Vst::IParamValueQueue
 * automation points into the host-neutral cvdsp::manager::ParameterManager<T>
 * automation API.
 *
 * The adapter performs no heap allocation, throws no exceptions, uses no RTTI,
 * owns no host state, creates no custom queues, applies no smoothing, performs
 * no interpolation, and forwards automation exclusively through
 * ParameterManager::enqueueAutomation(...).
 */

#include "../../Manager/ParameterManager.hpp"

#include <cstddef>

#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>

namespace cvdsp
{
namespace adapters
{
namespace vst3
{

/**
 * @brief Identity mapper from Steinberg::Vst::ParamID to CV_DSP ParameterID.
 *
 * This policy is suitable when the VST3 parameter IDs are intentionally kept in
 * sync with cvdsp::manager::ParameterID values. More advanced hosts can provide
 * a custom mapper object exposing the same map(paramID, parameterID) API.
 */
struct VST3IdentityParameterIDMapper
{
    /**
     * @brief Maps a VST3 ParamID to a CV_DSP ParameterID.
     */
    bool map(
        Steinberg::Vst::ParamID vstID,
        cvdsp::manager::ParameterID& parameterID) const noexcept
    {
        parameterID = static_cast<cvdsp::manager::ParameterID>(vstID);
        return true;
    }
};

/**
 * @brief Diagnostic counters returned by VST3ParameterAdapter operations.
 */
struct VST3ParameterAdapterResult
{
    std::size_t queuesVisited = 0;       ///< Number of VST3 parameter queues visited.
    std::size_t queuesRejected = 0;      ///< Number of queues rejected before point traversal.
    std::size_t pointsVisited = 0;       ///< Number of VST3 automation points visited.
    std::size_t pointsEnqueued = 0;      ///< Number of points accepted by ParameterManager.
    std::size_t pointsRejected = 0;      ///< Number of points rejected by adapter or manager.
    std::size_t invalidQueues = 0;       ///< Null or otherwise invalid VST3 queues.
    std::size_t invalidPoints = 0;       ///< Points with invalid VST3 data or failed getPoint calls.
    std::size_t invalidIDs = 0;          ///< Points/queues whose mapped CV_DSP ID was not registered.
    std::size_t notAutomatable = 0;      ///< Points rejected because parameter is not automatable.
    std::size_t invalidSampleOffsets = 0; ///< Points rejected due to invalid sample offsets.
    std::size_t queueFull = 0;           ///< Points rejected because manager event storage is full.
    cvdsp::manager::ParameterManagerStatus lastStatus =
        cvdsp::manager::ParameterManagerStatus::Ok; ///< Last manager status observed.
    bool hadOverflow = false;            ///< True once EventQueueFull is observed.

    /**
     * @brief Returns true when no point or queue was rejected.
     */
    bool success() const noexcept
    {
        return queuesRejected == 0
            && pointsRejected == 0
            && invalidQueues == 0
            && invalidPoints == 0
            && invalidIDs == 0
            && notAutomatable == 0
            && invalidSampleOffsets == 0
            && queueFull == 0;
    }
};

/**
 * @brief Converts VST3 parameter changes to CV_DSP ParameterManager automation.
 *
 * VST3ParameterAdapter is intentionally stateless. It does not allocate, cache,
 * smooth, interpolate, or create a secondary automation queue. Every valid VST3
 * automation point is forwarded directly to ParameterManager::enqueueAutomation.
 */
class VST3ParameterAdapter final
{
public:
    VST3ParameterAdapter() = delete;
    VST3ParameterAdapter(const VST3ParameterAdapter&) = delete;
    VST3ParameterAdapter& operator=(const VST3ParameterAdapter&) = delete;
    ~VST3ParameterAdapter() = delete;

    /**
     * @brief Returns true when a VST3 IParameterChanges pointer is non-null.
     */
    static bool isValidChanges(
        Steinberg::Vst::IParameterChanges* changes) noexcept
    {
        return changes != nullptr;
    }

    /**
     * @brief Returns true when a VST3 IParamValueQueue pointer is non-null.
     */
    static bool isValidQueue(
        const Steinberg::Vst::IParamValueQueue* queue) noexcept
    {
        return queue != nullptr;
    }

    /**
     * @brief Returns true when a VST3 point count is positive.
     */
    static constexpr bool hasPoints(
        Steinberg::int32 pointCount) noexcept
    {
        return pointCount > 0;
    }

    /**
     * @brief Returns true when a VST3 sample offset is non-negative.
     */
    static constexpr bool isValidSampleOffset(
        Steinberg::int32 sampleOffset) noexcept
    {
        return sampleOffset >= 0;
    }

    /**
     * @brief Converts a VST3 sample offset to std::size_t after validation.
     */
    static constexpr std::size_t toSampleOffset(
        Steinberg::int32 sampleOffset) noexcept
    {
        return isValidSampleOffset(sampleOffset)
            ? static_cast<std::size_t>(sampleOffset)
            : static_cast<std::size_t>(0);
    }

    /**
     * @brief Adapts ProcessData::inputParameterChanges using identity ID mapping.
     */
    template<typename T, std::size_t MaxParameters, std::size_t MaxAutomationEvents>
    static VST3ParameterAdapterResult adaptProcessData(
        const Steinberg::Vst::ProcessData& data,
        cvdsp::manager::ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager) noexcept
    {
        return adaptProcessData(
            data,
            manager,
            VST3IdentityParameterIDMapper());
    }

    /**
     * @brief Adapts ProcessData::inputParameterChanges using a custom mapper.
     */
    template<
        typename T,
        std::size_t MaxParameters,
        std::size_t MaxAutomationEvents,
        typename Mapper>
    static VST3ParameterAdapterResult adaptProcessData(
        const Steinberg::Vst::ProcessData& data,
        cvdsp::manager::ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager,
        const Mapper& mapper) noexcept
    {
        return adaptParameterChanges(
            data.inputParameterChanges,
            manager,
            mapper);
    }

    /**
     * @brief Adapts IParameterChanges using identity ID mapping.
     */
    template<typename T, std::size_t MaxParameters, std::size_t MaxAutomationEvents>
    static VST3ParameterAdapterResult adaptParameterChanges(
        Steinberg::Vst::IParameterChanges* changes,
        cvdsp::manager::ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager) noexcept
    {
        return adaptParameterChanges(
            changes,
            manager,
            VST3IdentityParameterIDMapper());
    }

    /**
     * @brief Adapts IParameterChanges using a custom mapper.
     */
    template<
        typename T,
        std::size_t MaxParameters,
        std::size_t MaxAutomationEvents,
        typename Mapper>
    static VST3ParameterAdapterResult adaptParameterChanges(
        Steinberg::Vst::IParameterChanges* changes,
        cvdsp::manager::ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager,
        const Mapper& mapper) noexcept
    {
        VST3ParameterAdapterResult result;

        if (!isValidChanges(changes))
            return result;

        const Steinberg::int32 queueCount = changes->getParameterCount();
        if (queueCount <= 0)
            return result;

        for (Steinberg::int32 queueIndex = 0; queueIndex < queueCount; ++queueIndex)
        {
            Steinberg::Vst::IParamValueQueue* queue = changes->getParameterData(queueIndex);
            ++result.queuesVisited;

            if (!isValidQueue(queue))
            {
                ++result.invalidQueues;
                ++result.queuesRejected;
                continue;
            }

            adaptQueue(*queue, manager, mapper, result);

            if (result.hadOverflow)
                break;
        }

        return result;
    }

    /**
     * @brief Adapts one IParamValueQueue using identity ID mapping.
     */
    template<typename T, std::size_t MaxParameters, std::size_t MaxAutomationEvents>
    static VST3ParameterAdapterResult adaptQueue(
        Steinberg::Vst::IParamValueQueue& queue,
        cvdsp::manager::ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager) noexcept
    {
        VST3ParameterAdapterResult result;
        ++result.queuesVisited;
        adaptQueue(queue, manager, VST3IdentityParameterIDMapper(), result);
        return result;
    }

    /**
     * @brief Adapts one IParamValueQueue using a custom mapper.
     */
    template<
        typename T,
        std::size_t MaxParameters,
        std::size_t MaxAutomationEvents,
        typename Mapper>
    static VST3ParameterAdapterResult adaptQueue(
        Steinberg::Vst::IParamValueQueue& queue,
        cvdsp::manager::ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager,
        const Mapper& mapper) noexcept
    {
        VST3ParameterAdapterResult result;
        ++result.queuesVisited;
        adaptQueue(queue, manager, mapper, result);
        return result;
    }

private:
    /**
     * @brief Adapts one queue and accumulates diagnostics in result.
     */
    template<
        typename T,
        std::size_t MaxParameters,
        std::size_t MaxAutomationEvents,
        typename Mapper>
    static void adaptQueue(
        Steinberg::Vst::IParamValueQueue& queue,
        cvdsp::manager::ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager,
        const Mapper& mapper,
        VST3ParameterAdapterResult& result) noexcept
    {
        cvdsp::manager::ParameterID parameterID = 0;
        if (!mapper.map(queue.getParameterId(), parameterID))
        {
            ++result.invalidIDs;
            ++result.queuesRejected;
            return;
        }

        const Steinberg::int32 pointCount = queue.getPointCount();
        if (!hasPoints(pointCount))
            return;

        for (Steinberg::int32 pointIndex = 0; pointIndex < pointCount; ++pointIndex)
        {
            ++result.pointsVisited;

            Steinberg::int32 sampleOffset = 0;
            Steinberg::Vst::ParamValue normalizedValue = 0.0;
            const Steinberg::tresult pointResult = queue.getPoint(
                pointIndex,
                sampleOffset,
                normalizedValue);

            if (pointResult != Steinberg::kResultOk
                || !isValidSampleOffset(sampleOffset))
            {
                ++result.invalidPoints;
                ++result.pointsRejected;
                continue;
            }

            const cvdsp::manager::ParameterManagerStatus status = manager.enqueueAutomation(
                parameterID,
                toSampleOffset(sampleOffset),
                static_cast<T>(normalizedValue));

            result.lastStatus = status;
            handleStatus(status, result);

            if (status == cvdsp::manager::ParameterManagerStatus::EventQueueFull)
                break;
        }
    }

    /**
     * @brief Accumulates diagnostics for one ParameterManager status.
     */
    static void handleStatus(
        cvdsp::manager::ParameterManagerStatus status,
        VST3ParameterAdapterResult& result) noexcept
    {
        switch (status)
        {
            case cvdsp::manager::ParameterManagerStatus::Ok:
                ++result.pointsEnqueued;
                break;

            case cvdsp::manager::ParameterManagerStatus::InvalidID:
            case cvdsp::manager::ParameterManagerStatus::InvalidIndex:
                ++result.invalidIDs;
                ++result.pointsRejected;
                break;

            case cvdsp::manager::ParameterManagerStatus::NotAutomatable:
                ++result.notAutomatable;
                ++result.pointsRejected;
                break;

            case cvdsp::manager::ParameterManagerStatus::InvalidSampleOffset:
                ++result.invalidSampleOffsets;
                ++result.pointsRejected;
                break;

            case cvdsp::manager::ParameterManagerStatus::EventQueueFull:
                ++result.queueFull;
                ++result.pointsRejected;
                result.hadOverflow = true;
                break;

            case cvdsp::manager::ParameterManagerStatus::Full:
            case cvdsp::manager::ParameterManagerStatus::InvalidDescriptor:
            case cvdsp::manager::ParameterManagerStatus::DuplicateID:
            default:
                ++result.pointsRejected;
                break;
        }
    }
};

} // namespace vst3
} // namespace adapters
} // namespace cvdsp

#endif // CVDSP_ADAPTERS_VST3_VST3PARAMETERADAPTER_HPP
