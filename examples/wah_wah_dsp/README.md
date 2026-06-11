# Wah-Wah DSP smoke example

This standalone smoke example verifies that `WahWahDSP` can be included,
prepared, configured, swept with expression values and used to process mono
buffers without producing NaN/Inf values.

It intentionally stays outside the VST3 examples until the pedal family API is
stable.

Manual compile check from the repository root:

```bash
c++ -std=c++20 -Wall -Wextra -pedantic -I. examples/wah_wah_dsp/wah_wah_smoke.cpp -o /tmp/wah_wah_smoke && /tmp/wah_wah_smoke
```
