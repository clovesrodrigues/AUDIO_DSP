# CV_DSP Control

This folder contains real-time-safe control generators and feature extractors
that are not audio effects by themselves. They are intended to drive parameters
of pedals, filters and modulation processors without depending on VST3, JUCE,
CLAP or any other host SDK.

## Modules

- `ExpressionEngine.hpp`: deterministic expression generator that analyzes a
  mono guitar/audio block, tracks envelope/transient density plus host BPM/PPQ,
  and outputs a normalized expression value suitable for `WahWahDSP` or other
  expression-controlled processors.
