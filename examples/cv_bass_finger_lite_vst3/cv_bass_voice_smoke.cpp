#include "CV_DSP/Synthesis/Bass/BassFingerVoice.hpp"
#include "CV_DSP/Reverb/RoomReverb.hpp"
#include "CV_DSP/Core/AudioBufferView.hpp"
#include "CV_DSP/Core/ProcessContext.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

int main()
{
    cvdsp::synthesis::bass::BassFingerVoice<float> voice;
    voice.prepare(48000.0f);

    voice.setTone(0.65f);
    voice.setAttackMs(2.0f);
    voice.setVelocitySensitivity(1.0f);
    voice.setCompression(0.35f);
    voice.setDrive(0.1f);
    voice.setBassGainDb(2.0f);
    voice.setMidGainDb(1.0f);
    voice.setTrebleGainDb(-1.0f);
    voice.setFingerNoise(0.25f);
    voice.setHumanize(0.2f);
    voice.setOutputGainDb(-6.0f);

    voice.noteOn(40, 0.8f, false);
    assert(voice.isActive());
    assert(voice.getCurrentNote() == 40);
    assert(std::abs(voice.getFrequency() - 82.4069f) < 0.05f);

    float peak = 0.0f;
    for (int i = 0; i < 2048; ++i)
        peak = std::max(peak, std::abs(voice.processSample()));

    assert(peak > 0.001f);

    voice.reset();
    voice.setVelocitySensitivity(1.0f);
    voice.noteOn(40, 0.15f, false);
    float softPeak = 0.0f;
    for (int i = 0; i < 2048; ++i)
        softPeak = std::max(softPeak, std::abs(voice.processSample()));
    assert(peak > softPeak);

    voice.noteOn(43, 0.7f, true);
    assert(voice.isActive());
    assert(voice.getCurrentNote() == 43);
    assert(std::abs(voice.getFrequency() - 97.9989f) < 0.05f);

    float left[64] {};
    float right[64] {};
    float* channels[2] {left, right};
    cvdsp::AudioBufferView<float> roomBuffer(channels, 2, 64);
    cvdsp::ProcessContext<float> roomContext {};
    roomContext.sampleRate = 48000.0f;
    roomContext.numChannels = 2;
    cvdsp::reverb::RoomReverb<float> room;
    room.prepare(roomContext);
    room.setWet(0.08f);
    room.processBlock(roomBuffer);

    voice.noteOff();
    for (int i = 0; i < 12000; ++i)
        (void)voice.processSample();

    assert(!voice.isActive());

    return 0;
}
