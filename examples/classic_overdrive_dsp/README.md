# Classic Overdrive DSP smoke example

This minimal standalone smoke example verifies that `ClassicOverdriveDSP` can be
included, prepared, configured and used to process a mono buffer without
producing NaN/Inf values.

It is intentionally not a VST3 target yet. The VST3/GUI adapter will be added in
a later phase after the pedal family stabilizes.

Manual compile check from the repository root:

```bash
c++ -std=c++20 -Wall -Wextra -pedantic -I. examples/classic_overdrive_dsp/classic_overdrive_smoke.cpp -o /tmp/classic_overdrive_smoke && /tmp/classic_overdrive_smoke
```
