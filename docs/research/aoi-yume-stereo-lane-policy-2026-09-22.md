# Aoi YUME voice-lane stereo gain policy

Date: 2026-09-22

## Why this changed

The first stereo pass (see `aoi-yume-stereoization-2026-09-22.md`) spread mono
SF2 voices over fixed lanes, but its gain law was inconsistent. The centre lane
returned `1.0 / 1.0`, while every other lane returned plain equal-power gains of
about `0.707 / 0.707`. In channel-summed terms the centre lane carried twice the
power of any other lane, so as soon as the spread left zero the non-centre
voices dropped about 3 dB. The lane table was also unbalanced: its eight
positions summed to `-0.52`, biasing the image to the left.

## Policy now in force

`VoicePool::stereoGainsForVoice()` owns the whole policy, and `renderStereo()`
uses nothing else:

- Nine lanes, indexed by voice slot. Lane 0 is the centre; the rest run outward
  as mirrored pairs `+/-0.30`, `+/-0.48`, `+/-0.14`, `+/-0.38`. The position set
  sums to exactly zero, so filling every lane leaves both channels at the same
  power. No magnitude exceeds `0.5`, so even at full width the pan value stays
  inside `0.02 ... 0.98` and never saturates against the clamp.
- Every lane uses the same equal-power law scaled by `sqrt(2)`. A centred voice
  therefore returns `0.99999994 / 0.99999994` (unity to float precision) and
  every lane carries the same total power `left^2 + right^2 = 2`, which is the
  power of the legacy dual-mono signal.
- Width `w` scales the lane position before panning. Zero and negative widths
  collapse all lanes to the centre, so `w = 0` reproduces the legacy mono render
  and small widths stay continuous with it.
- Lane assignment follows the voice slot, and `findFreeVoice()` always takes the
  lowest idle slot. The first voice of a fresh phrase therefore lands on the
  centre lane and stays dual-mono, while a monophonic line played on an idle
  pool keeps every note centred instead of wandering across the image.
- A voice's position is fixed when it is rendered, and stealing a slot does not
  change the assigned lane, so no voice moves inside a note. A voice that is
  alone only because its neighbours released keeps its position until it ends.

The internal spread constant stays `0.60` in `PluginProcessor.cpp`, unchanged,
as do all parameter IDs and `PLUGIN_CODE`. At that width a three-note chord sits
at `0.681 / 1.239` on its outer lanes (about 5.2 dB of separation) while the
first voice stays dual-mono; the widest lane reaches roughly 9 dB.

## Evidence (this checkout, this machine)

- `cmake --build --preset plugin`: success; Release-arm64 VST3/AU/Standalone
  relinked and ad-hoc signed.
- `ctest --preset plugin --output-on-failure -E pluginval`: **166/166 passed**,
  including the new `[stereo]` cases.
- `ctest --preset dev`: **88/88 passed**.
- `'build-plugin/plugins/rompler/test/rompler_tests' '[stereo]'`: 8 cases,
  2645 assertions, all passed.
- `ctest --preset plugin --output-on-failure -R pluginval_vst3_strictness10`:
  **passed** in 278.66 s against the VST3 bundle below.

Release VST3 component binary SHA-256:

`05ae66a23f1562972d596cf13b5d2a8e82f5a0c270c10885c7d7a9184ac65990`

The previous stereo pass reported
`8c9344ac9978cf0ce87809e05a280a9049c13b7e16fb789a2090e8ca1b6c0353` for the
same bundle path; the new hash supersedes it for any host or listening evidence.

New or extended regression coverage in `plugins/rompler/test/test_stereo_render.cpp`:

| Case | What it pins down |
|---|---|
| equal-power, balanced lanes | every lane's `left^2 + right^2` equals 2; the nine lanes balance L against R; lane 0 is unity and dual-mono; lane indices wrap; zero/negative widths collapse to the centre |
| single centred voice | one voice on lane 0 renders bit-identical L and R at full spread |
| near-zero width | width `1e-5` stays within `1e-4` of the legacy dual-mono render, which fails on the old 3 dB step |
| mono host and oversized block | a null right channel and a block larger than the prepared scratch buffer both fall back to the dual-mono signal, bit-identical to the mono render, with zero allocations |

The pre-existing width-zero equivalence and no-allocation cases still pass; the
width-zero case now exercises the real gain path instead of a dedicated fallback
branch.

## Still open

- pluginval strictness 10, host loading, automation/recall, and listening remain
  the project's separate gates; pluginval now passes for this hash, while host
  loading, recall and listening are still unverified for it.
- The added spread is audible only through a stereo output. A mono host, or a
  host block larger than the prepared buffer, still renders dual-mono by design.
- Whether the default spread depth is right for the shipped banks is a listening
  decision, and the plan in `aoi-yume-quality-review-plan-2026-09-22.md` keeps a
  user-facing Width control behind separate compatibility review.
