# Vintage Hard Distortion DSP smoke example

This standalone smoke example verifies that `VintageHardDistortionDSP` can be
prepared, configured and processed with oversampling Off/2x/4x/8x without
producing NaN/Inf values.

It intentionally stays outside the VST3 examples until the pedal family API is
stable.

Manual compile check from the repository root:

```bash
c++ -std=c++20 -Wall -Wextra -pedantic -I. examples/vintage_hard_distortion_dsp/vintage_hard_distortion_smoke.cpp -o /tmp/vintage_hard_distortion_smoke && /tmp/vintage_hard_distortion_smoke
```
