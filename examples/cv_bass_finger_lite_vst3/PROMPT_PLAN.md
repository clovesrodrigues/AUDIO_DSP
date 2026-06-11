# CV Bass Finger Lite prompt plan

This file records the agreed staged plan for the first lightweight bass instrument. The goal is to avoid scope creep: no sampler engine, no heavy physical modeling, no convolution body modeling, no custom GUI, and no CPU-expensive ambience in this first pass.

## Prompt status

1. **VST3 instrument skeleton** — create a minimal VST3 instrument target that can be built and loaded.
2. **Neutral MIDI layer** — translate host events into CV_DSP-owned MIDI/event data without SDK dependencies in the DSP core.
3. **Monophonic note tracking** — add fixed-capacity last-note-priority note handling for bass-style playing.
4. **First playable bass voice** — render a simple monophonic tone from MIDI with note on/off behavior.
5. **Expressive voice controls** — add velocity response, legato behavior, dynamic tone filtering, output gain, and basic parameter plumbing.
6. **Practical internal shaping** — add drive, compression, and 3-band EQ so the bass can reach a usable sound without external effects.
7. **Humanized details** — add subtle finger attack/release noise and deterministic per-note variation.
8. **Optional ambience** — add a short low-CPU room blend, disabled/near-free when the mix is off.
9. **Closure and usage guide** — document the final signal path, recommended Reaper use, safe starting settings, and where to stop before starting a new feature cycle.

## Current prompt

Prompt 9 is the final prompt of this first cycle. After this point, new work should be treated as a new cycle, not as automatic continuation of this prompt list.

## Recommended next cycle candidates

If the instrument is playable in Reaper and CPU use is acceptable, the next cycle should be chosen deliberately from one of these options:

- preset polishing for GM-style bass flavors;
- MIDI expression support such as pitch bend range and modulation mapping;
- better release/slide articulations;
- packaging/install notes for the target operating system;
- measured CPU profiling and optimization.

Do not add all of these at once.

## Post-cycle correction

After the first listening pass, the clean default sound was prioritized over the more obvious synthetic effects. Drive, finger noise, and room should remain optional controls, not mandatory defaults.
