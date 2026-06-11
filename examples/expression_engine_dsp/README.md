# Expression Engine DSP smoke example

This standalone smoke example verifies that `ExpressionEngine` can analyze audio
blocks, generate finite normalized expression values, and drive `WahWahDSP`
without producing NaN/Inf values.

Manual compile check from the repository root:

```bash
c++ -std=c++20 -Wall -Wextra -pedantic -I. examples/expression_engine_dsp/expression_engine_smoke.cpp -o /tmp/expression_engine_smoke && /tmp/expression_engine_smoke
```
