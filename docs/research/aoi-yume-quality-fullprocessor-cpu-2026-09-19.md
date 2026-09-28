# Aoi YUME full-processor sustained CPU baseline

## Why this exists

The stage-1 diagnostic measured `VoicePool::render` in isolation on a
synthetic 997 Hz sample transposed across the whole keyboard. It left the
"sustained full-processor CPU headroom" gate open. This benchmark measures
the complete `processBlock` chain — parameter sync, note-queue and MIDI
dispatch, voice rendering, stereo spread, the oversampled bus stage,
dynamics, FX, output gain, and the safety ceiling — under sustained real
SoundFont polyphony.

## Method

`plugins/rompler/test/test_full_processor_cpu.cpp`, hidden behind
`[.][benchmark]` so it never runs in the default ctest pass:

```sh
build-plugin/plugins/rompler/test/rompler_tests 'full processor sustained CPU deadline baseline'
```

- Font: `Crystal Legacy.sf2` (the shipped default; falls back to FFVIGM /
  Triton Strings / Dr. Mario when absent). The test scans every preset and
  picks the one with the most keys mapping to **loop-enabled, audible**
  samples (peak > 0.05). Non-looping samples are unusable: a high note
  plays them back many times faster, so a sample that looks long enough at
  rate 1 can still exhaust mid-run — that produced `active=123/128` on the
  first draft of this test.
- Load: `allNotesOff` + N `noteOn`s (velocity 100), distributed across the
  selected patch's key range, at position 0 of the first warmup block, then
  8 warmup + 80 timed `processBlock` calls with empty MIDI. Sustain level
  100% keeps every voice in sustain for the
  whole timed window. `getActiveVoiceCountForTesting()` verifies the pool
  stayed full; the maximum peak across all timed blocks and both channels
  verifies that the render stayed audible
  (the 128-voice configs sit on the 0.985 output-safety ceiling, so the
  stage-2 protection is exercised too).
- Parameters per config: `poly.limit` = target voices, `bus.osFactor` ∈
  {1x, 4x}, `voice.drive` ∈ {0, 12}. All other params at defaults
  (chorus 8%, reverb 10%, comp dry).

Evidence identity for the rerun: macOS 15.7.9 on Mac13,2 (20 logical CPUs),
direct `RomplerProcessor::processBlock` harness with no DAW host, at 48/96 kHz
and 64/512-sample blocks. The VST3 executable used by the build was
`0a5f2e737cb12f6446ba9f1ed4b106a00019a14420dd8246e106be42fc7385ba` (SHA-256).

## Results (2026-09-21 rerun; median µs over 80 timed blocks)

The full matrix (2 rates × 2 block sizes × 2 voice counts × 2 OS factors ×
2 drive settings) passed its active-voice, finite-output, and deadline checks:
32/32 configurations recorded `0/80` misses. The rows below show the
representative 4x / drive-12 cases.

| Rate | Block | Voices | OS | Drive | Median | p95 | Deadline | Missed |
|---|---|---|---|---|---:|---:|---:|---:|
| 48k | 64 | 32 | 4x | 12 | 116 | 123 | 1333 | 0/80 |
| 48k | 64 | 128 | 4x | 12 | 446 | 513 | 1333 | 0/80 |
| 48k | 512 | 32 | 4x | 12 | 869 | 957 | 10667 | 0/80 |
| 48k | 512 | 128 | 4x | 12 | 3254 | 3416 | 10667 | 0/80 |
| 96k | 64 | 32 | 4x | 12 | 121 | 132 | 667 | 0/80 |
| 96k | 64 | 128 | 4x | 12 | 432 | 478 | 667 | 0/80 |
| 96k | 512 | 32 | 4x | 12 | 872 | 964 | 5333 | 0/80 |
| 96k | 512 | 128 | 4x | 12 | 3180 | 3382 | 5333 | 0/80 |

## Findings

1. **The full chain met the deadline in all 32 configurations in this rerun.**
   The worst median corner — 96 kHz / 128 voices / 4x / drive 12 — sits at
   ~60% of the block budget. Timing is machine-load dependent, so this is a
   machine-specific observation rather than a portable pass/fail contract.
   The stage-1 misses (77/80 and 70/80) were produced by the synthetic
   extreme-transposition workload (997 Hz sine spread across notes
   0-127, pushing voices into the most expensive interpolator rate
   brackets), not by the rest of the chain. Per-voice cost is dominated
   by playback-rate bracket, not voice count alone.
2. **Oversampling is not the dominant cost.** 1x vs 4x differs by only
   ~3-7% median; voice rendering dwarfs the halfband chain. Drive 12 adds
   ~5%. Chain fixed overhead (OS + dynamics + FX + safety) is roughly
   ~60-90 µs per block at 32 voices.
3. **96k/64/128 remains the closest short-block edge** (~65% used at 4x / drive
   12). If headroom is ever needed there, the lever is voice cost
   (rate bracket / polyphony), not the post-voice chain.
4. `peak=0.985` on 128-voice configs confirms the OutputSafety ceiling is
   reached under full polyphony — the stage-2 protection engages in the
   measured path.

This closes the "sustained full-processor CPU headroom" measurement gap.
DAW insertion and listening remain separate user gates.
