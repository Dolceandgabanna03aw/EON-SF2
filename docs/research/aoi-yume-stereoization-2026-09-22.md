# Aoi YUME synthetic stereoization

Date: 2026-09-22

## Result

The SF2 samples remain mono. The processor now creates a stereo image at the
voice-mix stage instead of copying one final mono sum to both output channels.
`VoicePool::renderStereo()` renders each active voice once into a prepared
scratch buffer, then adds that voice to L/R with a deterministic equal-power
lane position. The first lane stays centred; later lanes alternate left and
right. The processor applies the existing CC10 global pan after this mix.

The initial internal spread is `0.60`. No existing parameter ID or
`PLUGIN_CODE` was changed, and the old mono `VoicePool::render()` API remains
available. A later user-facing Stereo Width control can scale the same lane
positions after its parameter compatibility is reviewed.

The scratch buffer is allocated by `prepareToPlay()`. The audio path has a
dual-mono fallback for mono hosts, oversized blocks, or an unprepared pool;
that fallback does not allocate.

## Verification

- `ctest --preset dev --output-on-failure`: 88/88 passed.
- `ctest --preset plugin --output-on-failure -E pluginval`: 162/162 passed.
- `ctest --preset plugin --output-on-failure -R pluginval_vst3_strictness10`:
  passed in 275.95 s.
- Direct `[benchmark]` run: 144 assertions passed, including full-processor
  CPU cases at 48/96 kHz, 64/512 samples, 32/128 voices, and 1x/4x
  oversampling; all full-processor deadline counters were `0/80`.
- Stereo unit tests cover width-zero dual-mono equivalence, multi-voice L/R
  separation, and zero allocations after prepare.
- SF2 integration test covers a real three-note chord with chorus, reverb, and
  delay disabled and observes a non-zero sample-level L/R difference.

Ableton Live host execution evidence (2026-09-22):

- Live rescanned plug-ins with `Use VST3 Plug-In System Folders` enabled.
- The browser showed one `Aoi YUME` result with the `VST3` filter selected.
- Double-clicking it created an `Aoi YUME` MIDI track and loaded the device;
  the device was active and routed to `Main` (Live's `1/2` output).
- Live reported `48.0 kHz`. Computer MIDI Keyboard input was sent to the
  armed track; the MIDI input indicator updated and the track peak reached
  `-3.14 dB`.
- The Live CPU meter stayed around `12–13%` during this short execution check.
- REAPER was intentionally not used for this pass.

Release VST3 component binary SHA-256:

`8c9344ac9978cf0ce87809e05a280a9049c13b7e16fb789a2090e8ca1b6c0353`

## Remaining proof gate

Ableton loading and MIDI execution are now evidenced. Separate host gates still
remain for inspecting independent L/R channel meters and for listening; the
automated evidence above does not replace those checks.

## Ableton-only follow-up check

Use an overlapping three-note chord for the host check. A single voice is kept
centred by design, so one isolated note is expected to remain dual-mono. Record
or monitor the Aoi YUME track as a stereo `Main 1/2` signal and compare the two
channels while the chord is held. Then move the existing CC10/global pan and
confirm that the whole image moves without collapsing the individual voice
spread. Listen once for note stealing and release tails at the selected Live
buffer size.
