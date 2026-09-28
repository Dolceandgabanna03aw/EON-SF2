# Aoi YUME output headroom probe

Date: 2026-09-22. Checkout `codex/mkii` + working tree, Release-arm64 VST3
SHA-256 `05ae66a23f1562972d596cf13b5d2a8e82f5a0c270c10885c7d7a9184ac65990`.

## Question

Stage 2 added a soft ceiling after the FX chain (`knee 0.97`, asymptote
`0.985`). A protection stage should almost never engage on ordinary playing. If
factory patches at their default settings spend time against that ceiling, then
the ceiling is doing level control and its soft clipping is part of the sound.

## Method

Hidden benchmark `plugins/rompler/test/test_output_headroom_benchmark.cpp`
(`[.][benchmark]`, so it never runs in the default ctest pass). For six presets
spread across `Crystal Legacy.sf2` (54 presets) it renders, through the complete
`processBlock` chain at default parameters:

- a three-note chord (keys at 1/4, 1/2 and 3/4 of the preset's sounding range)
  at velocity 100, and
- the highest sampled key at velocity 127.

Each probe flushes sounding notes, waits 375 silent blocks (~2 s at 48 kHz / 256)
so the previous probe's reverb tail cannot leak in, triggers, then measures 32
blocks (171 ms), counting samples at or above the knee and at the ceiling.
It repeats the render with Output Trim offsets of 0, -3, -6 and -12 dB from the
shipped default (-3 dB) and reports the smallest cut that keeps the whole signal
below the knee. The probe asserts that no rendered block ever exceeds the
ceiling, that every timed window carries signal the trigger itself produced, and
that a clean trim always exists.

## Results

`HEADROOM-MIN` is the extra trim below the shipped default that removes the
last knee crossing. Counts are out of 16384 samples per case.

| Preset (bank/program) | Scenario | Trim | Peak | At knee | At ceiling | Extra trim needed |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| M1 Legacy Piano (0/0) | chord | -3 dB | 0.985 | 147 | 100 | -6 dB |
| M1 Legacy Piano (0/0) | loud top | -3 dB | 0.757 | 0 | 0 | 0 dB |
| Flute (0/74) | chord | -3 dB | 0.985 | 1954 | 1589 | -6 dB |
| Flute (0/74) | loud top | -3 dB | 0.985 | 24 | 0 | -3 dB |
| M1 Legacy Guitar (0/28) | chord | -3 dB | 0.985 | 44 | 31 | -3 dB |
| M1 Legacy Guitar (0/28) | loud top | -3 dB | 0.731 | 0 | 0 | 0 dB |
| Mute Bass (2/27) | chord | -3 dB | 0.985 | 7 | 2 | -3 dB |
| Mute Bass (2/27) | loud top | -3 dB | 0.561 | 0 | 0 | 0 dB |
| Synth Bass (0/40) | chord | -3 dB | 0.985 | 24 | 13 | -3 dB |
| Synth Bass (0/40) | loud top | -3 dB | 0.454 | 0 | 0 | 0 dB |
| Timpani (0/48) | chord | -3 dB | 0.727 | 0 | 0 | 0 dB |
| Timpani (0/48) | loud top | -3 dB | 0.670 | 0 | 0 | 0 dB |

Five of six presets reach the knee on a three-note chord, and two of them reach
the ceiling: the Flute chord sits against the ceiling for 1589 samples, 9.7% of
the measured window. Removing the safety stage's influence needs 3 dB on four
patches and 6 dB on the piano and flute chords. Single fortissimo top notes are
clean apart from one 24-sample knee crossing on the flute.

## Reading

The cause is the sum, not the patches individually: nothing normalises the voice
mix, so three voices add roughly 9.5 dB over one voice, and the default trim
(-3 dB) leaves the peak above the knee. The safety stage then shapes the loudest
part of every chord rather than only catching accidents.

That is a sound decision, not a bug, so it is deliberately left alone here. The
options this evidence supports, pending a level-matched listening comparison:

1. Lower the shipped default Output Trim, which is the smallest change but
   alters every existing session that uses the default.
2. Reduce factory preset gains, which changes existing presets individually.
3. Add a bounded voice-count or programme-dependent gain stage, which is the
   largest change and needs its own headroom and listening evidence.

## Measurement bug found and fixed

The first version of this probe read its residual with
`std::max (buffer.getMagnitude (0, kBlockSize), buffer.getMagnitude (1, kBlockSize))`.
`AudioBuffer` has no `(channel, numSamples)` overload: that call is
`(startSample, numSamples)` across every channel. The second call therefore read
one float past the end of the allocation and returned heap contents - values as
large as `6.65e28` appeared in the log while the buffer's own samples read
`7e-8`. The plugin was never at fault: an isolated 200-allocation check of the
same API returned exactly the manual per-sample scan.

The same idiom appears twice in the working-tree tests and is now corrected:

- `plugins/rompler/test/test_full_processor_cpu.cpp` - the peak accumulator now
  calls `getMagnitude (0, blockSize)` once, which already spans both channels.
- `plugins/rompler/test/test_sf2_playback.cpp` - the per-channel peaks now use
  the three-argument form `getMagnitude (channel, 0, kBlockSize)`.

The previously recorded peak columns from that CPU benchmark remain plausible
(the out-of-bounds float can only have inflated them, and they already sat on the
ceiling), but any peak number measured with the old idiom should be treated as
provisional until re-run with the corrected calls.

## Evidence and remaining gates

- `'build-plugin/plugins/rompler/test/rompler_tests' 'representative preset output headroom probe'`:
  73 assertions, all passed, no block exceeded the ceiling.
- `ctest --preset plugin --output-on-failure -E pluginval`: **166/166 passed**.
- `ctest --preset plugin --output-on-failure -R pluginval_vst3_strictness10`:
  **passed** in 278.66 s for the hash above.
- Host loading, automation/recall and listening remain separate gates, and the
  choice between the three options above needs a level-matched listening check.
