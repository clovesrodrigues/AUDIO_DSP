#ifndef CV_OBS_PLUGIN_INTERNAL_GAIN_PROCESSOR_HPP
#define CV_OBS_PLUGIN_INTERNAL_GAIN_PROCESSOR_HPP

#include "CV_DSP/Core/AudioBufferView.hpp"

#include <cstddef>

namespace cv_obs_plugin
{

class InternalGainProcessor
{
public:
    void process(cvdsp::AudioBufferView<float> buffer, float linearGain) const noexcept
    {
        if (!buffer.isValid())
            return;

        for (std::size_t channel = 0; channel < buffer.getNumChannels(); ++channel) {
            float* samples = buffer.getChannel(channel);
            if (!samples)
                continue;

            for (std::size_t sample = 0; sample < buffer.getNumSamples(); ++sample)
                samples[sample] *= linearGain;
        }
    }
};

} // namespace cv_obs_plugin

#endif // CV_OBS_PLUGIN_INTERNAL_GAIN_PROCESSOR_HPP
