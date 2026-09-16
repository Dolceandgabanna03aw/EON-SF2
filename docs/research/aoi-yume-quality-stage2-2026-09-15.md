# Aoi YUME quality stage 2: polyphonic output headroom

## Root cause

The expanded EONQC matrix found a repeatable failure only in the three-note
`chord` scenario. The post-voice chorus/reverb and polyphonic sum reached
`+1.7517`, `+1.8569`, and `+1.9620 dBFS` at 44.1/48/96 kHz respectively.
Other scenarios remained finite and below the peak gate. The original
`out.trim` default is `-3 dB`; there was no final output ceiling after FX.

## Test-first investigation

`test_output_safety.cpp` was added before the implementation. The initial
build failed because `OutputSafetyProcessor.h` did not exist. After the first
JUCE limiter prototype, a regression test caught undocumented makeup gain:
an input sample of `0.5` became `0.484991729`. That prototype was removed.
The final tests require both overload protection and exact unity for normal
material, plus zero allocations after prepare.

## Implementation

`OutputSafetyProcessor` runs after the existing output trim and FX chain in
`RomplerProcessor::renderRange`. It applies a bounded, allocation-free soft
ceiling only above `0.97`, asymptotically approaching `0.985`. Values below
the knee are unchanged; no parameter IDs, presets, reported latency, or
global makeup gain were changed. The stage is sample-level protection, not a
true-peak oversampled limiter.

## Final evidence

Final VST3 executable SHA-256:

`f2d98a72dad8b3ead3f10cc940545e6d1239feb3932220ac34153d38dac3f0d5`

Codesign verification passed. `ctest --preset plugin --output-on-failure`
passed **159/159**, including `pluginval_vst3_strictness10` in 282.83 s.

The three EONQC stress contracts were re-prepared against the final hash;
all 24 scenarios passed. Chord output was:

| Matrix | Report peak | Raw WAV peak | Exact full-scale samples |
| --- | ---: | ---: | ---: |
| 44.1 kHz / 256 | -0.131275 dBFS | 0.9850000 | 0 |
| 48 kHz / 512 | -0.131275 dBFS | 0.9850000 | 0 |
| 96 kHz / 128 | -0.131275 dBFS | 0.9850000 | 0 |

The original five-scenario Clear/Character EONQC render also passed. Peaks
remain at the prior levels: Clear C4 `-2.481617 dBFS`, Clear C7
`-5.431271 dBFS`, Character C4 `-6.796822 dBFS`, and Character C7
`-7.585257 dBFS`; silence remained `-240 dBFS` with zero non-finite samples.

Artifacts:

- Stress reports: `/Users/sungha/Documents/Codex/2026-09-02/find-sound-quality-check-tool-2/outputs/aoi-yume-stress-44k256`, `aoi-yume-stress-48k512`, `aoi-yume-stress-96k128`
- Static report: `/Users/sungha/Documents/Codex/2026-09-02/find-sound-quality-check-tool-2/outputs/aoi-yume-2026-09-15-stage3`

DAW insertion/listening and sustained full-processor CPU headroom remain
separate gates. The previous 96 kHz / 128-voice sampler benchmark still
exceeded its short-block deadline; this output stage does not claim to fix
that independent limit.
