# Aoi YUME Clear and Character Design

## Understanding summary

- Aoi YUME remains one VST3/AU/Standalone product; there is no Mk II plugin
  identity or duplicate DAW scan target in this change.
- The product needs both a clear, modern virtual-instrument sound and a
  textured, more coloured alternative.
- `Clear` is the intended new-instance/default direction: clean attack,
  controlled low-mid build-up, modest stereo movement, and short, bright space.
- `Character` is a factory profile for intentional tone colour, not a hidden
  second signal path.
- All audible profile settings must be represented by the existing visible
  knobs. Selecting a profile must therefore change real APVTS values rather
  than introduce invisible DSP multipliers.
- Existing parameter IDs, plugin code, real-time safety rules, and host latency
  reporting remain unchanged.
- Build, unit tests, pluginval, DAW loading, and DAW listening remain separate
  evidence gates.

## Assumptions and non-goals

- “VST concept” means a polished, clear virtual-instrument default, rather
  than permanently vintage or hardware-style coloration.
- The user explicitly authorised changing default timbre. Existing saved
  sessions retain their saved parameter values; this change does not migrate or
  reinterpret parameter IDs.
- No new macro, tone-mode parameter, or duplicate plugin is added. A macro
  would conceal the relationship between panel state and audible output.
- This is not a loudness-maximisation project: Clear stays at -3 dB and no
  limiter or hard clipper is introduced. Character uses its visible output
  trim at 0 dB only to compensate for its colour, compression, and ambience.

## Decision log

| Decision | Alternatives considered | Rationale |
| --- | --- | --- |
| One product, two factory profiles | Separate Mk II plugin | Preserves DAW identity, scanning, sessions, and preset workflows. |
| Profiles set existing parameters | A hidden Clear/Character macro | The visible knob values remain the source of truth for sound and automation. |
| Clear defaults reduce modulation and reverb | Add a permanent exciter/EQ stage | Avoids a new opaque DSP stage and preserves a direct SF2 signal. |
| Character uses moderate colour only | Make Fold a default texture | Fold is an explicit effect; default use would obscure transients and pitch detail. |

## Final design

The packaged preset library gains two read-only factory entries using the
bundled `Crystal Legacy.sf2` source:

| Profile | Intent | Key settings |
| --- | --- | --- |
| `Aoi Clear` | New clear/modern reference | Drive/Tape/Fold = 0; cutoff = 20 kHz; resonance = 0; 4x OS; compressor mix = 0; Chorus = 0.65 Hz / 14% depth / 8% mix; Reverb = 28% room / 32% damping / 10% mix; Output = -3 dB. |
| `Aoi Character` | Warm, textured alternative | Voice Drive = 12%; Tape = 14%; Fold = 0; cutoff = 17 kHz; resonance = 12%; 4x OS; compressor = -18 dB / 2.5:1 / 24 ms / 220 ms / +1.5 dB / 28% mix; Chorus = 0.8 Hz / 24% depth / 17% mix; Reverb = 42% room / 52% damping / 16% mix; Output = 0 dB (level-matched to Clear). |

`Aoi Clear` values also become the new APVTS defaults for the high-frequency
space controls (chorus rate/depth/mix and reverb room/damping/mix). The
existing `Bus Filter Cutoff` exponential range and fixed 4x oversampling
default are retained: their response is already appropriate for fine control
without changing saved automation semantics. Drive, tape, resonance, and fold
stay deliberately separated: colour is progressively applied by distinct,
visible controls instead of a single overloaded “quality” knob.

## Verification strategy

1. Prove the new Clear parameter defaults with a processor test.
2. Prove both bundled `.eonpreset` documents parse and expose the expected
   factory category and real parameter values.
3. Build the plugin preset and run the targeted test, full rompler tests,
   `ctest --preset plugin`, and strictness-10 pluginval.
4. Rebuild the bundle and confirm the profiles are copied into
   `Contents/Resources/Presets`.
5. The user verifies DAW scanning and a Clear-vs-Character listening A/B;
   those observations are not inferred from the automated tests.
