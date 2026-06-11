# Sustainer DSP smoke example

This standalone smoke example verifies that `SustainerDSP` can be included,
prepared, configured, used to process a mono guitar-like buffer and used on
silence/noise without producing NaN/Inf values.

It intentionally stays outside the VST3 examples until the pedal family API is
stable.

Manual compile check from the repository root:

```bash
c++ -std=c++20 -Wall -Wextra -pedantic -I. examples/sustainer_dsp/sustainer_smoke.cpp -o /tmp/sustainer_smoke && /tmp/sustainer_smoke
```
