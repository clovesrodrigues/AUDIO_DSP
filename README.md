# AUDIO_DSP Repository Structure

## 📁 Organization

This repository contains a professional C++ DSP (Digital Signal Processing) library for audio.
All components are organized under the `CV_DSP` namespace.

### Directory Layout

```
├── .github/
│   └── FUNDING.yml
├── CV_DSP/
│   ├── Convolution/
│   │   ├── ConvolutionEngine.hpp
│   │   └── IRLoader.hpp
│   ├── Core/
│   │   ├── AudioBufferView.hpp
│   │   ├── CircularBuffer.hpp
│   │   ├── Config.hpp
│   │   ├── Constants.hpp
│   │   ├── DSPUtils.hpp
│   │   ├── Namespace.hpp
│   │   ├── ParameterSmoother.hpp
│   │   ├── ProcessContext.hpp
│   │   ├── Types.hpp
│   │   └── Version.hpp
│   ├── Delay/
│   │   └── DelayLine.hpp
│   ├── Dynamics/
│   │   ├── Compressor.hpp
│   │   ├── EnvelopeFollower.hpp
│   │   ├── Expander.hpp
│   │   ├── Limiter.hpp
│   │   └── NoiseGate.hpp
│   ├── Effects/
│   │   ├── Chorus.hpp
│   │   └── Flanger.hpp
│   ├── Filters/
│   │   ├── AllPassFilter.hpp
│   │   ├── Biquad.hpp
│   │   ├── DCBlocker.hpp
│   │   ├── LadderFilter.hpp
│   │   ├── OnePoleFilter.hpp
│   │   └── StateVariableFilter.hpp
│   ├── Guitar/
│   │   └── CabinetSimulator.hpp
│   ├── Math/
│   │   ├── FastMath.hpp
│   │   ├── Interpolation.hpp
│   │   ├── LookupTable.hpp
│   │   └── Oversampling.hpp
│   ├── Modulation/
│   │   ├── ADSR.hpp
│   │   ├── LFO.hpp
│   │   └── Oscillator.hpp
│   ├── Saturation/
│   │   ├── TapeSaturation.hpp
│   │   ├── TubeSaturation.hpp
│   │   └── Waveshaper.hpp
│   ├── Spatial/
│   │   ├── MidSide.hpp
│   │   └── StereoWidth.hpp
│   └── Spectral/
│       ├── FFT.hpp
│       ├── SpectrumAnalyzer.hpp
│       ├── STFT.hpp
│       └── WindowFunctions.hpp
├── LICENSE
└── README.md
```
and more... 

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

CV_DSP/Core
Header	Classes / structs / aliases relevantes
AudioBufferView.hpp	AudioBufferView<T>, ConstAudioBufferView<T> 
CircularBuffer.hpp	CircularBuffer<T, Capacity> 
Config.hpp	Flags globais como EnableAssertions e EnableDenormalProtection 
Constants.hpp	Constantes matemáticas template
DSPUtils.hpp	DSPUtils 
Namespace.hpp	Macros de portabilidade CVDSP_FORCE_INLINE, CVDSP_NODISCARD; namespace cvdsp 
ParameterSmoother.hpp	LinearSmoother<T>, ExponentialSmoother<T>, OnePoleSmoother<T> 
ProcessContext.hpp	ProcessContext<T> 
Types.hpp	f32, f64, i32, u32, i64, u64 
Version.hpp	Informações de versão
CV_DSP/Math
Header	Classes / structs / enums relevantes
FastMath.hpp	Funções matemáticas rápidas em namespace cvdsp
Interpolation.hpp	LinearInterpolation, CubicInterpolation, HermiteInterpolation, CatmullRomInterpolation, LagrangeInterpolation
LookupTable.hpp	LookupTableFunction, LookupTableCapacity, LookupTable<T, Capacity>
Oversampling.hpp	OversamplingFactor, HalfbandFIR<T>, Oversampling<T, Factor> 
CV_DSP/Filters
Header	Classes / structs / enums relevantes
AllPassFilter.hpp	AllPassFilter<T>
Biquad.hpp	BiquadType, Biquad<T> 
DCBlocker.hpp	DCBlocker<T>
LadderFilter.hpp	LadderFilter<T>
OnePoleFilter.hpp	OnePole<T>, LowPassOnePole<T>, HighPassOnePole<T>
StateVariableFilter.hpp	SVFMode, StateVariableFilter<T>
Observação arquitetural: alguns filtros estão em cvdsp::filters, como Biquad. 

CV_DSP/Dynamics
Header	Classes / structs / enums relevantes
Compressor.hpp	Compressor<T> 
EnvelopeFollower.hpp	EnvelopeMode, EnvelopeFollower<T>
Expander.hpp	Expander<T>
Limiter.hpp	Limiter<T>
NoiseGate.hpp	GateState, NoiseGate<T>
Observação: Compressor<T> já tem prepare(), reset(), setters, process() e processBlock(), então a camada Processing deve adaptar esse padrão em vez de substituí-lo. 

CV_DSP/Delay
Header	Classes / structs / enums relevantes
DelayLine.hpp	InterpolationType, DelayLine<T, MaxDelaySamples, Interpolation>
CV_DSP/Effects
Header	Classes / structs / enums relevantes
Chorus.hpp	Chorus<T> 
Flanger.hpp	Flanger<T>
Observação: Chorus<T> segue o padrão clássico da biblioteca: prepare(), reset(), setters e process() sample a sample. 

CV_DSP/Modulation
Header	Classes / structs / enums relevantes
ADSR.hpp	ADSRState, ADSR<T>
LFO.hpp	LFOWaveform, LFO<T>
Oscillator.hpp	OscillatorWaveform, Oscillator<T>
Esses módulos devem alimentar futuramente uma ModulationMatrix<T>.

CV_DSP/Saturation
Header	Classes / structs / enums relevantes
TapeSaturation.hpp	TapeSaturation<T>
ToneStack.hpp	ToneStack<T>
TubeSaturation.hpp	TubeSaturation<T>
Waveshaper.hpp	WaveshaperMode, Waveshaper<T>
CV_DSP/Spatial
Header	Classes / structs relevantes
MidSide.hpp	MidSide<T>, MSFrame<T>, StereoFrame<T>
StereoWidth.hpp	StereoWidth<T>, StereoFrame<T>
CV_DSP/Spectral
Header	Classes / structs / enums relevantes
FFT.hpp	FFT<T>
STFT.hpp	STFTMode, STFT<T, FFTSize, OverlapPercent>
SpectrumAnalyzer.hpp	SpectrumAnalyzer<T, FFTSize>
WindowFunctions.hpp	WindowType, WindowFunctions<T, N>
CV_DSP/Convolution
Header	Classes / structs relevantes
ConvolutionEngine.hpp	ConvolutionEngine<T, FFTSize, MaxIRSamples> 
IRLoader.hpp	IRLoader<T>, RIFFHeader, ChunkHeader, FormatChunk
CV_DSP/Guitar
Header	Classes / structs / enums relevantes
AmpSimulator.hpp	AmpSimulator<T, ...>
CabinetSimulator.hpp	CabinetSimulator<T, ...>
FenderToneStack.hpp	FenderToneStack<T>
MarshallToneStack.hpp	MarshallToneStack<T>
MesaToneStack.hpp	MesaToneStack<T>
PentodeStage.hpp	PentodeStage<T>
PowerAmp.hpp	PowerAmp<T>, PowerAmp<T>::Model
ToneStack.hpp	wrapper/adaptação sobre saturation tone stack
TriodeStage.hpp	TriodeStage<T>
TubePreamp.hpp	TubePreamp<T>
VoxToneStack.hpp	VoxToneStack<T>

A camada Guitar é uma composição de módulos existentes. Por exemplo, AmpSimulator.hpp inclui TubePreamp, ToneStack, MarshallToneStack, PowerAmp, CabinetSimulator e NoiseGate, com uma arquitetura de alto nível já parcialmente composta.
