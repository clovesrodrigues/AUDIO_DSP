# AUDIO_DSP Repository Structure

## 📁 Organization

This repository contains a professional C++ DSP (Digital Signal Processing) library for audio.
All components are organized under the `CV_DSP` namespace.

## 🎯 Integration Graph

### Real-Time Safe Audio Chain

```
Input Signal
    |
    v
  LFO                  ParameterSmoother
 (Modulation)           (Parameter Automation)
    |                         |
    +----> Chorus <-----------+
            (DelayLine + LFO)
                |
                v
         Compressor
      (Dynamics Processing)
                |
                v
          Output Signal
```

### Component Dependencies

| Component | Depends On | Purpose |
|-----------|-----------|----------|
| **DelayLine** | CircularBuffer | Fractional delay with 3 interpolation types |
| **Chorus** | DelayLine, LFO | Delay modulation effect |
| **LFO** | None | Independent oscillator |
| **Compressor** | EnvelopeFollower, ParameterSmoother | Dynamic range control |
| **ParameterSmoother** | None | Independent automation |
| **EnvelopeFollower** | ? | Needs verification |

### Correct Usage

```cpp
// ✓ CORRECT - Use CV_DSP namespace files
#include "CV_DSP/Core/CircularBuffer.hpp"
#include "CV_DSP/Delay/DelayLine.hpp"
#include "CV_DSP/Core/ParameterSmoother.hpp"
#include "CV_DSP/Modulation/LFO.hpp"
```

### DO NOT USE

```cpp
// ✗ WRONG - These files were removed
#include "Core/ParameterSmoother.hpp"      // Forward header - REMOVED
#include "Core/CircularBuffer.hpp"         // Duplicate - REMOVED
#include "Delay/DelayLine.hpp"             // Outdated - REMOVED
```

## 📊 Integration Improvements

### 1. Compressor with ParameterSmoother

The Compressor now integrates with ParameterSmoother for smooth parameter automation:

```cpp
// Before: Abrupt parameter changes
compressor.setThreshold(newThreshold);

// After: Smooth automation over N samples
compressor.getThresholdSmoother().setTarget(newThreshold, 480);  // 10ms @ 48kHz
```

### 2. Delay-Based Effects

All delay effects use DelayLine with optional ParameterSmoother:

- Chorus: DelayLine (modulado por LFO)
- Flanger: DelayLine (delay muito curto)
- Vibrato: DelayLine (pitch modulation)
- Reverb: Multi-tap DelayLine

## 🚀 Usage Examples

### Simple Chorus Effect

```cpp
#include "CV_DSP/Delay/DelayLine.hpp"
#include "CV_DSP/Modulation/LFO.hpp"

cvdsp::delay::DelayLine<float, 16384, cvdsp::delay::InterpolationType::Linear> chorus;
chorus.prepare(48000);

cvdsp::LFO<float> lfo;
lfo.prepare(48000.0f, 2.5f, 1.0f, cvdsp::LFOWaveform::Sine);

for (size_t n = 0; n < blockSize; ++n) {
    float lfoValue = lfo.process();
    float delay = 40.0f + lfoValue * 10.0f;  // 40±10 ms
    chorus.setDelayMilliseconds(delay);
    
    float output = input[n] + 0.7f * chorus.process(input[n]);
}
```

### Compressor with Smooth Parameter Changes

```cpp
#include "CV_DSP/Dynamics/Compressor.hpp"
#include "CV_DSP/Core/ParameterSmoother.hpp"

cvdsp::Compressor<float> compressor;
compressor.prepare(48000);

// Smooth parameter automation
compressor.getThresholdSmoother().setTarget(-20.0f, 480);  // Ramp over 10ms

for (size_t n = 0; n < blockSize; ++n) {
    compressor.setThreshold(compressor.getThresholdSmoother().process());
    output[n] = compressor.process(input[n]);
}
```

## 📝 Migration Guide

If you were using the old forward headers:

### Before

```cpp
#include "Core/ParameterSmoother.hpp"   // Forward to CV_DSP
```

### After

```cpp
#include "CV_DSP/Core/ParameterSmoother.hpp"  // Direct include
```

## ✨ Quality Assurance

- ✓ No duplicate headers
- ✓ All components in CV_DSP namespace
- ✓ Real-time safe (O(1) per sample for process())
- ✓ Header-only implementations
- ✓ Zero dynamic allocation during audio processing
- ✓ Professional documentation with examples

## 📚 References

- **CircularBuffer**: Ring buffer for real-time audio storage
- **DelayLine**: Fractional delay with Hermite interpolation
- **ParameterSmoother**: Linear/Exponential/OnePole automation
- **LFO**: Sine/Triangle/Saw/Square waveforms (0.01-50 Hz)
- **Chorus**: Classic chorus effect (Haas effect)
