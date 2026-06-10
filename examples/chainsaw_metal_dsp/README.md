# Chainsaw Metal DSP smoke example

This standalone smoke example verifies that `ChainsawMetalDSP` can process all
voice modes with oversampling Off/2x/4x/8x, extreme gain/threshold/mid-boost
settings, silence and descriptor validation without producing NaN/Inf values.

Manual compile check from the repository root:

```bash
c++ -std=c++20 -Wall -Wextra -pedantic -I. examples/chainsaw_metal_dsp/chainsaw_metal_smoke.cpp -o /tmp/chainsaw_metal_smoke && /tmp/chainsaw_metal_smoke
```
