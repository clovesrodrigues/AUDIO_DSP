#ifndef CVDSP_MANAGER_PARAMETERMANAGER_HPP
#define CVDSP_MANAGER_PARAMETERMANAGER_HPP

/**
 * @file ParameterManager.hpp
 * @brief Host-neutral fixed-capacity parameter registry and automation manager.
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * ParameterManager owns a fixed-capacity set of ParameterDescriptor and
 * ParameterState objects. It performs descriptor registration, ID/index lookup,
 * sample-accurate neutral automation dispatch, per-sample state advancement, and
 * preset-friendly snapshot capture/restoration without depending on any host SDK.
 *
 * The manager performs no dynamic allocation, uses no RTTI, throws no exceptions,
 * and does not include or reference Steinberg, JUCE, CLAP, iPlug2, or any other
 * framework. Host adapters are expected to translate their native parameter IDs
 * and automation queues into the neutral ParameterID/index and normalized-value
 * events accepted by this class.
 */

#include "../Core/Namespace.hpp"
#include "../Core/ProcessContext.hpp"
#include "../Core/Types.hpp"
#include "ParameterDescriptor.hpp"
#include "ParameterState.hpp"

namespace cvdsp::manager
{

/**
 * @brief Result code for parameter registration and event insertion.
 */
enum class ParameterManagerStatus : u32
{
    Ok = 0,             ///< Operation completed successfully.
    Full,               ///< Fixed-capacity storage is full.
    InvalidDescriptor,  ///< Descriptor is invalid.
    DuplicateID,        ///< A parameter with the same ID is already registered.
    InvalidIndex,       ///< Parameter index is outside the registered range.
    InvalidID,          ///< Parameter ID was not found.
    NotAutomatable,     ///< Parameter policy rejects host automation.
    EventQueueFull,     ///< Fixed-capacity automation queue is full.
    InvalidSampleOffset ///< Automation event sample offset is outside current block.
};

/**
 * @brief Neutral sample-accurate automation event.
 *
 * Events are stored by resolved parameter index for real-time dispatch. The ID is
 * retained for diagnostics, snapshots, and adapter-facing APIs.
 */
template<typename T = f32>
struct ParameterAutomationEvent
{
    ParameterID id = 0;                 ///< Neutral parameter ID.
    std::size_t parameterIndex = 0;     ///< Resolved internal parameter index.
    std::size_t sampleOffset = 0;       ///< Block-relative sample offset.
    T normalizedValue = static_cast<T>(0); ///< Target value in normalized 0..1 form.
    u32 rampSamples = 0;                ///< Optional explicit ramp length.
    bool hasExplicitRamp = false;       ///< Whether rampSamples should be used.
};

/**
 * @brief One neutral parameter snapshot entry.
 */
template<typename T = f32>
struct ParameterSnapshotEntry
{
    ParameterID id = 0;                    ///< Neutral parameter ID.
    T normalizedValue = static_cast<T>(0); ///< Stored target normalized value.
};

/**
 * @brief Fixed-capacity neutral parameter snapshot.
 */
template<typename T = f32, std::size_t MaxEntries = 128>
struct ParameterSnapshot
{
    u32 version = 1;                            ///< Snapshot format version.
    std::size_t count = 0;                      ///< Number of valid entries.
    ParameterSnapshotEntry<T> entries[MaxEntries]{}; ///< Snapshot entries.

    /**
     * @brief Clears all entries without changing capacity.
     */
    CVDSP_FORCE_INLINE void clear() noexcept
    {
        count = 0;
    }

    /**
     * @brief Returns true when an entry can be appended.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool hasCapacity() const noexcept
    {
        return count < MaxEntries;
    }
};

/**
 * @brief Fixed-capacity manager for CV_DSP parameters.
 *
 * @tparam T Floating-point value type, normally cvdsp::f32 or cvdsp::f64.
 * @tparam MaxParameters Maximum number of registered parameters.
 * @tparam MaxAutomationEvents Maximum number of queued automation events per block.
 */
template<
    typename T = f32,
    std::size_t MaxParameters = 128,
    std::size_t MaxAutomationEvents = 1024>
class ParameterManager
{
    static_assert(MaxParameters > 0, "ParameterManager requires MaxParameters > 0");
    static_assert(MaxAutomationEvents > 0, "ParameterManager requires MaxAutomationEvents > 0");

public:
    using value_type = T;
    using descriptor_type = ParameterDescriptor<T>;
    using state_type = ParameterState<T>;
    using smoothing_config_type = ParameterSmoothingConfig<T>;
    using automation_event_type = ParameterAutomationEvent<T>;

    static constexpr std::size_t InvalidIndex = static_cast<std::size_t>(-1);

public:
    /**
     * @brief Constructs an empty parameter manager.
     */
    constexpr ParameterManager() noexcept = default;

    /**
     * @brief Registers a descriptor and creates its runtime state.
     *
     * Registration is intended for setup, not the audio callback. The descriptor
     * is copied into stable fixed storage before ParameterState binds to it.
     */
    CVDSP_NODISCARD ParameterManagerStatus registerParameter(
        const descriptor_type& descriptor,
        const ParameterSmoothingMode smoothingMode = ParameterSmoothingMode::None,
        const smoothing_config_type& smoothingConfig = smoothing_config_type{}) noexcept
    {
        if (!descriptor.isValid())
            return ParameterManagerStatus::InvalidDescriptor;

        if (contains(descriptor.getID()))
            return ParameterManagerStatus::DuplicateID;

        if (parameterCount_ >= MaxParameters)
            return ParameterManagerStatus::Full;

        const std::size_t index = parameterCount_;
        descriptors_[index] = descriptor;

        if (!states_[index].bind(&descriptors_[index]))
            return ParameterManagerStatus::InvalidDescriptor;

        states_[index].setSmoothingMode(smoothingMode);
        states_[index].prepare(isPrepared_ ? activeSmoothingConfig_ : smoothingConfig);

        ++parameterCount_;
        return ParameterManagerStatus::Ok;
    }

    /**
     * @brief Prepares all registered states from a ProcessContext.
     */
    CVDSP_NODISCARD bool prepare(
        const ProcessContext<T>& context,
        const smoothing_config_type& smoothingConfig = smoothing_config_type{}) noexcept
    {
        if (context.sampleRate <= static_cast<T>(0))
            return false;

        maxBlockSize_ = context.blockSize;
        currentBlockSize_ = context.blockSize;
        activeSmoothingConfig_ = smoothingConfig;
        activeSmoothingConfig_.sampleRate = context.sampleRate;
        prepareStates(activeSmoothingConfig_);
        isPrepared_ = true;
        clearAutomationEvents();
        currentSample_ = 0;
        return true;
    }

    /**
     * @brief Prepares all registered states from scalar timing values.
     */
    CVDSP_NODISCARD bool prepare(
        const T sampleRate,
        const std::size_t maxBlockSize,
        const smoothing_config_type& smoothingConfig = smoothing_config_type{}) noexcept
    {
        if (sampleRate <= static_cast<T>(0))
            return false;

        maxBlockSize_ = maxBlockSize;
        currentBlockSize_ = maxBlockSize;
        activeSmoothingConfig_ = smoothingConfig;
        activeSmoothingConfig_.sampleRate = sampleRate;
        prepareStates(activeSmoothingConfig_);
        isPrepared_ = true;
        clearAutomationEvents();
        currentSample_ = 0;
        return true;
    }

    /**
     * @brief Begins a processing block and clears block-local automation events.
     */
    void beginBlock(
        const std::size_t blockSize) noexcept
    {
        currentBlockSize_ = blockSize;
        currentSample_ = 0;
        clearAutomationEvents();
    }

    /**
     * @brief Begins a processing block using ProcessContext timing.
     */
    void beginBlock(
        const ProcessContext<T>& context) noexcept
    {
        currentBlockSize_ = context.blockSize;
        currentSample_ = 0;
        clearAutomationEvents();
    }

    /**
     * @brief Returns the number of registered parameters.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE std::size_t getParameterCount() const noexcept
    {
        return parameterCount_;
    }

    /**
     * @brief Returns the fixed parameter capacity.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr std::size_t getMaxParameters() const noexcept
    {
        return MaxParameters;
    }

    /**
     * @brief Returns true when the manager has been prepared.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isPrepared() const noexcept
    {
        return isPrepared_;
    }

    /**
     * @brief Returns the currently configured block size.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE std::size_t getCurrentBlockSize() const noexcept
    {
        return currentBlockSize_;
    }

    /**
     * @brief Finds a registered parameter index by neutral ID.
     */
    CVDSP_NODISCARD std::size_t findIndex(
        const ParameterID id) const noexcept
    {
        for (std::size_t i = 0; i < parameterCount_; ++i)
        {
            if (descriptors_[i].getID() == id)
                return i;
        }

        return InvalidIndex;
    }

    /**
     * @brief Returns true when a parameter ID is registered.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool contains(
        const ParameterID id) const noexcept
    {
        return findIndex(id) != InvalidIndex;
    }

    /**
     * @brief Returns a descriptor by index, or nullptr when invalid.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE const descriptor_type* getDescriptorByIndex(
        const std::size_t index) const noexcept
    {
        return index < parameterCount_ ? descriptors_ + index : nullptr;
    }

    /**
     * @brief Returns a descriptor by ID, or nullptr when not found.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE const descriptor_type* getDescriptor(
        const ParameterID id) const noexcept
    {
        const std::size_t index = findIndex(id);
        return index != InvalidIndex ? getDescriptorByIndex(index) : nullptr;
    }

    /**
     * @brief Returns mutable state by index, or nullptr when invalid.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE state_type* getStateByIndex(
        const std::size_t index) noexcept
    {
        return index < parameterCount_ ? states_ + index : nullptr;
    }

    /**
     * @brief Returns immutable state by index, or nullptr when invalid.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE const state_type* getStateByIndex(
        const std::size_t index) const noexcept
    {
        return index < parameterCount_ ? states_ + index : nullptr;
    }

    /**
     * @brief Returns mutable state by ID, or nullptr when not found.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE state_type* getState(
        const ParameterID id) noexcept
    {
        const std::size_t index = findIndex(id);
        return index != InvalidIndex ? getStateByIndex(index) : nullptr;
    }

    /**
     * @brief Returns immutable state by ID, or nullptr when not found.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE const state_type* getState(
        const ParameterID id) const noexcept
    {
        const std::size_t index = findIndex(id);
        return index != InvalidIndex ? getStateByIndex(index) : nullptr;
    }

    /**
     * @brief Sets a parameter immediately by index using a normalized value.
     */
    CVDSP_NODISCARD bool setImmediateNormalizedByIndex(
        const std::size_t index,
        const T normalizedValue) noexcept
    {
        state_type* state = getStateByIndex(index);
        return state != nullptr && state->setImmediateNormalized(normalizedValue);
    }

    /**
     * @brief Sets a parameter immediately by ID using a normalized value.
     */
    CVDSP_NODISCARD bool setImmediateNormalized(
        const ParameterID id,
        const T normalizedValue) noexcept
    {
        const std::size_t index = findIndex(id);
        return index != InvalidIndex && setImmediateNormalizedByIndex(index, normalizedValue);
    }

    /**
     * @brief Sets a parameter immediately by index using a real value.
     */
    CVDSP_NODISCARD bool setImmediateRealByIndex(
        const std::size_t index,
        const T realValue) noexcept
    {
        state_type* state = getStateByIndex(index);
        return state != nullptr && state->setImmediateReal(realValue);
    }

    /**
     * @brief Sets a parameter immediately by ID using a real value.
     */
    CVDSP_NODISCARD bool setImmediateReal(
        const ParameterID id,
        const T realValue) noexcept
    {
        const std::size_t index = findIndex(id);
        return index != InvalidIndex && setImmediateRealByIndex(index, realValue);
    }

    /**
     * @brief Enqueues a host-automation event by ID using the default ramp.
     */
    CVDSP_NODISCARD ParameterManagerStatus enqueueAutomation(
        const ParameterID id,
        const std::size_t sampleOffset,
        const T normalizedValue) noexcept
    {
        const std::size_t index = findIndex(id);
        if (index == InvalidIndex)
            return ParameterManagerStatus::InvalidID;

        return enqueueAutomationByIndex(index, sampleOffset, normalizedValue);
    }

    /**
     * @brief Enqueues a host-automation event by ID using an explicit ramp length.
     */
    CVDSP_NODISCARD ParameterManagerStatus enqueueAutomation(
        const ParameterID id,
        const std::size_t sampleOffset,
        const T normalizedValue,
        const u32 rampSamples) noexcept
    {
        const std::size_t index = findIndex(id);
        if (index == InvalidIndex)
            return ParameterManagerStatus::InvalidID;

        return enqueueAutomationByIndex(index, sampleOffset, normalizedValue, rampSamples);
    }

    /**
     * @brief Enqueues a host-automation event by resolved parameter index.
     */
    CVDSP_NODISCARD ParameterManagerStatus enqueueAutomationByIndex(
        const std::size_t index,
        const std::size_t sampleOffset,
        const T normalizedValue) noexcept
    {
        return enqueueAutomationByIndex(index, sampleOffset, normalizedValue, 0, false);
    }

    /**
     * @brief Enqueues a host-automation event by resolved index with explicit ramp.
     */
    CVDSP_NODISCARD ParameterManagerStatus enqueueAutomationByIndex(
        const std::size_t index,
        const std::size_t sampleOffset,
        const T normalizedValue,
        const u32 rampSamples) noexcept
    {
        return enqueueAutomationByIndex(index, sampleOffset, normalizedValue, rampSamples, true);
    }

    /**
     * @brief Clears all queued automation events for the current block.
     */
    CVDSP_FORCE_INLINE void clearAutomationEvents() noexcept
    {
        eventCount_ = 0;
        nextEventIndex_ = 0;
    }

    /**
     * @brief Returns the number of queued automation events.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE std::size_t getAutomationEventCount() const noexcept
    {
        return eventCount_;
    }

    /**
     * @brief Applies automation events due at or before sampleIndex.
     */
    void applyAutomationAtSample(
        const std::size_t sampleIndex) noexcept
    {
        while (nextEventIndex_ < eventCount_
            && automationEvents_[nextEventIndex_].sampleOffset <= sampleIndex)
        {
            applyAutomationEvent(automationEvents_[nextEventIndex_]);
            ++nextEventIndex_;
        }
    }

    /**
     * @brief Advances every registered ParameterState by one sample.
     */
    void processSample() noexcept
    {
        applyAutomationAtSample(currentSample_);

        for (std::size_t i = 0; i < parameterCount_; ++i)
            (void)states_[i].processSample();

        ++currentSample_;
    }

    /**
     * @brief Advances all registered states for a complete parameter block.
     */
    void processBlockParameters(
        const std::size_t blockSize) noexcept
    {
        currentBlockSize_ = blockSize;
        currentSample_ = 0;

        for (std::size_t sample = 0; sample < blockSize; ++sample)
            processSample();
    }

    /**
     * @brief Returns current real value by index, or zero when invalid.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getCurrentRealByIndex(
        const std::size_t index) const noexcept
    {
        const state_type* state = getStateByIndex(index);
        return state != nullptr ? state->getCurrentReal() : static_cast<T>(0);
    }

    /**
     * @brief Returns current real value by ID, or zero when not found.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getCurrentReal(
        const ParameterID id) const noexcept
    {
        const std::size_t index = findIndex(id);
        return index != InvalidIndex ? getCurrentRealByIndex(index) : static_cast<T>(0);
    }

    /**
     * @brief Returns target normalized value by index, or zero when invalid.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getTargetNormalizedByIndex(
        const std::size_t index) const noexcept
    {
        const state_type* state = getStateByIndex(index);
        return state != nullptr ? state->getTargetNormalized() : static_cast<T>(0);
    }

    /**
     * @brief Returns target normalized value by ID, or zero when not found.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getTargetNormalized(
        const ParameterID id) const noexcept
    {
        const std::size_t index = findIndex(id);
        return index != InvalidIndex ? getTargetNormalizedByIndex(index) : static_cast<T>(0);
    }

    /**
     * @brief Resets every registered state to its descriptor default.
     */
    void resetAllToDefaults() noexcept
    {
        for (std::size_t i = 0; i < parameterCount_; ++i)
            (void)states_[i].resetToDefault();
    }

    /**
     * @brief Clears dirty flags in every registered state.
     */
    void clearAllDirty() noexcept
    {
        for (std::size_t i = 0; i < parameterCount_; ++i)
            states_[i].clearDirty();
    }

    /**
     * @brief Writes a neutral snapshot of target normalized values.
     *
     * When persistentOnly is true, parameters not marked Persistent are skipped.
     */
    template<std::size_t MaxEntries>
    CVDSP_NODISCARD bool writeSnapshot(
        ParameterSnapshot<T, MaxEntries>& snapshot,
        const bool persistentOnly = true) const noexcept
    {
        snapshot.clear();

        for (std::size_t i = 0; i < parameterCount_; ++i)
        {
            if (persistentOnly && !states_[i].isPersistent())
                continue;

            if (!snapshot.hasCapacity())
                return false;

            snapshot.entries[snapshot.count].id = descriptors_[i].getID();
            snapshot.entries[snapshot.count].normalizedValue = states_[i].getTargetNormalized();
            ++snapshot.count;
        }

        return true;
    }

    /**
     * @brief Applies a neutral snapshot immediately.
     */
    template<std::size_t MaxEntries>
    CVDSP_NODISCARD bool applySnapshot(
        const ParameterSnapshot<T, MaxEntries>& snapshot) noexcept
    {
        bool allApplied = true;

        for (std::size_t i = 0; i < snapshot.count; ++i)
        {
            if (!setImmediateNormalized(snapshot.entries[i].id, snapshot.entries[i].normalizedValue))
                allApplied = false;
        }

        return allApplied;
    }

private:
    void prepareStates(
        const smoothing_config_type& smoothingConfig) noexcept
    {
        for (std::size_t i = 0; i < parameterCount_; ++i)
            states_[i].prepare(smoothingConfig);
    }

    CVDSP_NODISCARD bool sampleOffsetIsValid(
        const std::size_t sampleOffset) const noexcept
    {
        return currentBlockSize_ == 0 || sampleOffset < currentBlockSize_;
    }

    CVDSP_NODISCARD ParameterManagerStatus enqueueAutomationByIndex(
        const std::size_t index,
        const std::size_t sampleOffset,
        const T normalizedValue,
        const u32 rampSamples,
        const bool hasExplicitRamp) noexcept
    {
        if (index >= parameterCount_)
            return ParameterManagerStatus::InvalidIndex;

        if (!states_[index].isAutomatable())
            return ParameterManagerStatus::NotAutomatable;

        if (!sampleOffsetIsValid(sampleOffset))
            return ParameterManagerStatus::InvalidSampleOffset;

        if (eventCount_ >= MaxAutomationEvents)
            return ParameterManagerStatus::EventQueueFull;

        const T safeNormalized = descriptors_[index].clampNormalized(normalizedValue);
        automation_event_type event{
            descriptors_[index].getID(),
            index,
            sampleOffset,
            safeNormalized,
            rampSamples,
            hasExplicitRamp};

        insertAutomationEvent(event);
        return ParameterManagerStatus::Ok;
    }

    void insertAutomationEvent(
        const automation_event_type& event) noexcept
    {
        std::size_t insertIndex = eventCount_;

        while (insertIndex > 0
            && automationEvents_[insertIndex - 1].sampleOffset > event.sampleOffset)
        {
            automationEvents_[insertIndex] = automationEvents_[insertIndex - 1];
            --insertIndex;
        }

        automationEvents_[insertIndex] = event;
        ++eventCount_;

        if (insertIndex < nextEventIndex_)
            ++nextEventIndex_;
    }

    void applyAutomationEvent(
        const automation_event_type& event) noexcept
    {
        state_type* state = getStateByIndex(event.parameterIndex);
        if (state == nullptr)
            return;

        if (event.hasExplicitRamp)
            (void)state->setTargetNormalized(event.normalizedValue, event.rampSamples);
        else
            (void)state->setTargetNormalized(event.normalizedValue);
    }

private:
    descriptor_type descriptors_[MaxParameters]{};
    state_type states_[MaxParameters]{};
    automation_event_type automationEvents_[MaxAutomationEvents]{};

    std::size_t parameterCount_ = 0;
    std::size_t eventCount_ = 0;
    std::size_t nextEventIndex_ = 0;
    std::size_t currentSample_ = 0;
    std::size_t currentBlockSize_ = 0;
    std::size_t maxBlockSize_ = 0;

    smoothing_config_type activeSmoothingConfig_{};
    bool isPrepared_ = false;
};

} // namespace cvdsp::manager

#endif // CVDSP_MANAGER_PARAMETERMANAGER_HPP
