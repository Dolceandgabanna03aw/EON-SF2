# Aoi YUME review fixes verification

## Changes

- Sample interpolation blends adjacent rate and fractional-phase kernels.
  Unity-speed reads at integer positions return the original source sample.
  Both playback and SF2 loading use the continuous rate accessor.
- The sinc window is centred on its interpolation position. A 48-tap kernel
  and 33 rate positions retain the folded-image rejection target while
  supporting continuous pitch changes.
- Voice Drive blends smoothly from dry over the first absolute 1 dB and
  smooths automated drive changes over 5 ms. Existing colour at 12 dB is
  retained; the transition applies to all three curves and either sign.

## Evidence

- Build: `cmake --build --preset plugin --parallel 8` passed.
- Quality tests: 8 cases, 432 assertions passed. Coverage includes all rate
  bracket boundaries at three phases and three frequencies, phase rollover,
  unity high-frequency preservation, both directions through Drive zero for
  all three curves, alias rejection, wanted partials, and render allocations.
- The 14 kHz source played one octave up produced a measured folded 20 kHz
  component of -94.4943 dB, against the -55 dB limit in that synthetic fixture.
- Initial full CTest: pluginval strictness 10 passed (277.12 seconds), but
  the old arbitrary-waveform zero-crossing pitch test failed. That test now
  retains the real loaded SF2 region pitch metadata with a known sine probe;
  its measured octave ratio is 2.0 and tolerance tightened from 0.2 to 0.02.
- After that test-only correction, all 154 non-pluginval CTest cases passed.
  The plugin binary did not change, so the prior strictness-10 result remains
  applicable. VST3 codesign verification and `git diff --check` also passed.
- EONQC prepare, actual VST3 host render, and contract-bound analysis passed
  all five scenarios at 48 kHz / 512 frames / stereo. No non-finite samples.

| Scenario | Note RMS dBFS | Peak dBFS |
| --- | ---: | ---: |
| Clear C4 | -15.7216 | -2.4816 |
| Clear C7 | -24.9669 | -5.4313 |
| Character C4 | -16.4478 | -6.7968 |
| Character C7 | -24.9612 | -7.5853 |

Silence remains zero (reported as -240 dBFS by the measurement floor).

Binary SHA-256: `4705f64f324d4e908e66fa2a157cdd5e0661fb9de400ab662986d7fc6a5d865a`

EONQC workspace:
`/Users/sungha/Documents/Codex/2026-09-02/find-sound-quality-check-tool-2`

Artifacts relative to that workspace:
`outputs/aoi-yume-2026-09-07-review-fixes/` contains `eonqc-run.json`,
`eonqc-evidence.json`, `report.json`, and input/output WAVs.

## Limits

EONQC scenarios exercise static profiles; pitch/drive transition regressions
are covered by the C++ tests. These results do not establish DAW listening
quality or real-time CPU headroom. Fractional reads now do more arithmetic;
the sampled validator stack showed SF2 rate conversion during state restore.
No system plugin installation was performed in this change.
