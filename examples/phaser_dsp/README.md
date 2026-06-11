# Phaser DSP smoke example

This standalone smoke example verifies that the generic `cvdsp::Phaser` and the
pedal-facing `PhaserDSP` wrapper can be included, prepared, configured and used
to process mono buffers without producing NaN/Inf values.

Manual compile check from the repository root:

```bash
c++ -std=c++20 -Wall -Wextra -pedantic -I. examples/phaser_dsp/phaser_smoke.cpp -o /tmp/phaser_smoke && /tmp/phaser_smoke
```
