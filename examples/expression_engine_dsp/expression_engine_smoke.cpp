#include "../../CV_DSP/Control/ExpressionEngine.hpp"
#include "../../CV_DSP/Guitar/Pedals/WahWahDSP.hpp"

#include <cmath>
#include <cstddef>

namespace
{
bool isFiniteBuffer(const float* buffer, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!std::isfinite(buffer[i]))
            return false;
    }
    return true;
}
} // namespace

int main()
{
    constexpr std::size_t kBlockSize = 64;
    constexpr std::size_t kNumBlocks = 48;
    constexpr float kSampleRate = 48000.0f;

    cvdsp::control::ExpressionEngine<float> engine;
    cvdsp::guitar::pedals::WahWahDSP<float> wah;

    engine.prepare(kSampleRate);
    engine.setSensitivity(0.6f);
    engine.setTransientSensitivity(0.65f);
    engine.setSubdivision(cvdsp::control::ExpressionSubdivision::Sixteenth);

    wah.prepare(kSampleRate);
    wah.setMix(1.0f);
    wah.setLevelDb(-3.0f);

    float block[kBlockSize] {};
    double ppq = 0.0;
    const double ppqPerBlock = static_cast<double>(kBlockSize) / (kSampleRate * 0.5);

    for (std::size_t blockIndex = 0; blockIndex < kNumBlocks; ++blockIndex)
    {
        for (std::size_t i = 0; i < kBlockSize; ++i)
        {
            const float n = static_cast<float>(blockIndex * kBlockSize + i);
            const bool transient = (i == 0) && (blockIndex % 3 == 0);
            const float pluck = transient ? 0.35f : 0.0f;
            const float sustain = std::sin(n * 0.025f) * 0.12f * std::exp(-static_cast<float>(blockIndex) / 80.0f);
            block[i] = pluck + sustain;
        }

        engine.updateFeatures(block, kBlockSize, 120.0, ppq);
        wah.setExpression(engine.getTargetExpression());

        float* channels[] { block };
        cvdsp::AudioBufferView<float> buffer(channels, 1, kBlockSize);
        wah.processBlock(buffer);

        if (!isFiniteBuffer(block, kBlockSize))
            return 1;

        const float expression = engine.process();
        if (!std::isfinite(expression) || expression < 0.0f || expression > 1.0f)
            return 2;

        ppq += ppqPerBlock;
    }

    float silence[kBlockSize] {};
    for (std::size_t i = 0; i < 256; ++i)
    {
        engine.updateFeatures(silence, kBlockSize, 120.0, ppq);
        ppq += ppqPerBlock;
    }

    if (engine.getState() != cvdsp::control::ExpressionState::Idle)
        return 3;

    return 0;
}
