# Aoi YUME quality stage 1

## Scope and implementation

The sampler now selects a contiguous 48-tap reader when the full kernel lies
inside the sample and active loop boundaries. Sample heads, tails, and loop
seams retain the original guarded reader. Coefficients and summation order
are unchanged. Stationary pitch reuses the block rate rather than evaluating
sin/pow per sample; the vibrato phase still advances while modulation is off.
No parameter IDs, presets, latency, or installed bundles were changed.

## CPU evidence

Run after building the plugin preset:

```sh
build-plugin/plugins/rompler/test/rompler_tests '[benchmark]'
```

Hidden diagnostic test: synthetic looping 997 Hz sample, 12 cents vibrato,
8 warmup blocks and 80 timed blocks, 44.1/48/96 kHz, 64/512 frames,
16/32/64/128 voices, Drive 0/12. This is VoicePool time, not the complete
processor or DAW. Timing assertions do not certify real-time safety.

Median microseconds (Drive 12):

| Rate / frames / voices | Before | After | Deadline |
| --- | ---: | ---: | ---: |
| 48k / 64 / 32 | 244.75 | 148.88 | 1333.33 |
| 48k / 512 / 64 | 4150.29 | 2610.33 | 10666.67 |
| 48k / 512 / 128 | 8557.83 | 5523.75 | 10666.67 |
| 96k / 512 / 128 | 8453.29 | 5580.38 | 5333.33 |

96k/512/128 still missed 77/80 deadlines, and 96k/64/128 missed 70/80.
Concurrent system load and short measurement duration limit these results.
Temporary raw logs: `/tmp/aoi-stage1-cpu-baseline.log` and
`/tmp/aoi-stage1-cpu-after.log`; rerun the diagnostic for local conditions.

## Character Drive decision

`tools/drive_quality_bench.cpp` compares direct Tanh, first-order ADAA,
four-point midpoint quadrature, and residual ADAA. At 48k, 5k sine,
amplitude 0.5 and +12 dB, direct alias RMS was -42.199 dBFS versus
-47.838 dBFS for ADAA. But ADAA attenuated a low-level 20k signal by
11.740 dB. Residual ADAA restored its small-signal response but changed the
driven fundamental by +1.30 dB relative to ADAA. Midpoint quadrature is
not true filtered oversampling, and was slower without better rejection.

Decision: retain the existing direct voice Drive, not ADAA. The probe is a
bounded Tanh experiment, not proof for Tube/Transformer or all input spectra.
Filtered oversampling needs a separate latency/CPU/level-matching design
and listening check before deployment.

## Validation

- Plugin build passed; 25008 quality assertions in 9 cases passed, including
  short/long loop and zero/nonzero vibrato block-partition invariance.
- Final non-pluginval CTest passed all 155 cases.
- CPU diagnostic passed 48 voice-count assertions (deadline failures above).
- Actual VST3 SHA-256:
  `00f5360d584c334bdfbef5740d602b6e2faedc19f28eff87b85c09aa042b69a3`.
- Initial full plugin CTest passed 155/155 including pluginval strictness 10
  (279.52 seconds). The extra loop-invariance test was then included in the
  final 155-case non-pluginval run; the plugin binary hash did not change.
- Independent read-only review found no regression in the two optimizations;
  targeted tests passed 8/8. Exhaustive boundary differential testing and
  malformed manually-created loop metadata are not covered.
- Expanded EONQC actual VST3 reports match the binary hash above. Eight
  scenarios at each of 44.1k/256, 48k/512, and 96k/128: 21/24 pass;
  chord fails the 0 dBFS peak gate at +1.752, +1.857, and +1.962 dBFS.
  These are the report's aggregate peak values, not true peaks; at 96k
  the individual left-channel sample peak reaches +1.984 dBFS.
  No threshold was loosened. This exposes an output-headroom issue, not
  a proven new regression from the CPU optimization. All reports are under
  `/Users/sungha/Documents/Codex/2026-09-02/find-sound-quality-check-tool-2/outputs/`
  in `aoi-yume-stress-44k256`, `aoi-yume-stress-48k512`, and
  `aoi-yume-stress-96k128`. Preserve their contracts/evidence and WAVs.
- EONQC extension: frame-addressed MIDI sequences (chords, repeated notes,
  CC and bend) and parameter automation; host splits blocks at event frames.
  Python suite passed 67 tests; host compiled and all three renders returned
  status 0. All 24 scenarios passed finite-sample checks. Host SHA-256:
  `7bdf61fb905c4014afc063002194e9ab0fd5ecccbc7d73a2bb512ad92040008a`.
- DAW insertion, listening, and sustained full-processor CPU remain unverified.
