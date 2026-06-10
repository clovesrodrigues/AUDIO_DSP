# Guitar Pedalboard DSP smoke example

This standalone smoke example includes `CV_DSP/Guitar/Pedals.hpp`, prepares all
four guitar pedal DSP classes, processes the same mono test buffer through each
pedal independently, and validates descriptor metadata.

Manual compile check from the repository root:

```bash
c++ -std=c++20 -Wall -Wextra -pedantic -I. examples/guitar_pedalboard_dsp/guitar_pedalboard_smoke.cpp -o /tmp/guitar_pedalboard_smoke && /tmp/guitar_pedalboard_smoke
```
