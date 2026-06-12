#ifndef CV_OBS_PLUGIN_OBS_AUDIO_BUFFER_ADAPTER_HPP
#define CV_OBS_PLUGIN_OBS_AUDIO_BUFFER_ADAPTER_HPP

#include <obs-module.h>

#include "CV_DSP/Core/AudioBufferView.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace cv_obs_plugin
{

class ObsAudioBufferAdapter
{
public:
    static constexpr std::size_t kMaxChannels = MAX_AV_PLANES;

    explicit ObsAudioBufferAdapter(obs_audio_data* audio) noexcept
    {
        reset(audio);
    }

    void reset(obs_audio_data* audio) noexcept
    {
        audio_ = audio;
        channels_.fill(nullptr);
        numChannels_ = 0;
        numFrames_ = 0;
        timestamp_ = 0;

        if (!audio_)
            return;

        numFrames_ = audio_->frames;
        timestamp_ = audio_->timestamp;

        for (std::size_t plane = 0; plane < kMaxChannels; ++plane) {
            if (!audio_->data[plane])
                continue;

            channels_[numChannels_] = reinterpret_cast<float*>(audio_->data[plane]);
            ++numChannels_;
        }
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return audio_ && numChannels_ > 0 && numFrames_ > 0;
    }

    [[nodiscard]] std::size_t numChannels() const noexcept
    {
        return numChannels_;
    }

    [[nodiscard]] std::size_t numFrames() const noexcept
    {
        return numFrames_;
    }

    [[nodiscard]] std::uint64_t timestamp() const noexcept
    {
        return timestamp_;
    }

    [[nodiscard]] cvdsp::AudioBufferView<float> audioView() noexcept
    {
        return cvdsp::AudioBufferView<float>(channels_.data(), numChannels_, numFrames_);
    }

private:
    obs_audio_data* audio_ = nullptr;
    std::array<float*, kMaxChannels> channels_ = {};
    std::size_t numChannels_ = 0;
    std::size_t numFrames_ = 0;
    std::uint64_t timestamp_ = 0;
};

} // namespace cv_obs_plugin

#endif // CV_OBS_PLUGIN_OBS_AUDIO_BUFFER_ADAPTER_HPP
