# Vintage Fuzz DSP smoke example

This standalone smoke example verifies that `VintageFuzzDSP` can process normal
and extreme fuzz settings, including bias, starve, asymmetry, foldback,
rectification and gate paths, without producing NaN/Inf values or runaway DC.

Manual compile check from the repository root:

```bash
c++ -std=c++20 -Wall -Wextra -pedantic -I. examples/vintage_fuzz_dsp/vintage_fuzz_smoke.cpp -o /tmp/vintage_fuzz_smoke && /tmp/vintage_fuzz_smoke
```
