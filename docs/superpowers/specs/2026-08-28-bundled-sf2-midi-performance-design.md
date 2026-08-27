# Bundled SoundFonts and MIDI performance controls

## Goal

Ship the three supplied SoundFonts inside every macOS plugin bundle, open that
folder by default from the SoundFont loader, and make the EON-DS50 playable
from both its UI controls and normal incoming MIDI performance messages.

The supplied fonts are:

- `Live HQ Natural SoundFont GM.sf2`
- `Sonic_Mania_-_Korg_M1_Legacy_Soundfont.sf2`
- `SGM-v2.01-NicePianosGuitarsBass-V1.2.sf2`

## Packaging and SoundFont loading

- VST3, AU, and Standalone bundles copy all three files to
  `Contents/Resources/SoundFonts` during the build.
- The M1 Legacy SoundFont remains the named startup default. Runtime lookup is
  deterministic and must not depend on directory iteration order.
- The processor exposes the bundle SoundFonts directory to the editor. The
  Load button begins in that folder when it exists, and otherwise uses JUCE's
  normal empty/default location.
- Missing source files fail the packaging configuration clearly rather than
  silently producing a partial release. Runtime failure to locate the bundle
  remains non-fatal: the plugin opens and can load a user-selected SF2.

## Performance architecture

Incoming host MIDI and UI controls share one block-boundary performance state.
The audio thread consumes MIDI events in sample order without allocation,
locking, filesystem access, or host parameter writes.

- Pitch wheel uses the MIDI 14-bit value and applies a fixed +/-2 semitone
  offset to every active voice.
- CC1 sets vibrato depth. Vibrato is a per-voice sine LFO at a fixed musical
  rate, applied as a pitch offset in the same playback-rate calculation as the
  pitch wheel. A UI modulation wheel writes the same CC1 state.
- CC7 and CC11 form master gain and expression gain respectively; both scale
  the rendered voice signal before bus processing.
- CC10 controls equal-power output panning before the stereo signal enters the
  bus stage.
- CC64 defers note-off releases while held, then releases all deferred notes
  when it returns below 64.
- CC65 enables legato/portamento behaviour. With it enabled and at least one
  key held, a new note reuses the active lead voice: its sample pitch changes
  without retriggering its amplitude envelope. With it disabled, existing
  polyphonic retrigger behaviour remains unchanged.
- CC71 maps to the existing voice filter offset/tone path, and CC74 maps to
  the existing bus filter cutoff path. Their conversions are explicit and
  clamped to the declared APVTS ranges.
- Unsupported controllers are ignored. Standard all-notes-off/all-sound-off
  messages are handled as immediate voice release/silence safeguards.

## UI

- Add hardware-style vertical pitch and modulation wheels next to the keyboard.
- Pitch wheel springs to centre when released; modulation wheel holds its
  position. Both repaint when matching MIDI input arrives.
- Add a visible Voice-section legato control. It and CC65 use the same
  performance mode; changing either updates the other at the next safe block
  boundary.

## Error handling and compatibility

- Clamp malformed MIDI note, pitch, and controller values before using them.
- Do not mutate APVTS from the audio callback. MIDI-controlled parameter
  targets are represented by dedicated atomics/state and read in the block.
- Existing sessions without saved new controls retain the previous audible
  defaults: centred pitch, zero vibrato, normal gain, centred pan, sustain off,
  and legato off.
- Existing UI/computer-keyboard note queue observes the same sustain, pitch,
  vibrato, and legato state as host MIDI notes.

## Tests and verification

- Add failing-first voice tests for pitch-wheel transposition, non-zero CC1
  vibrato, sustain deferral/release, and legato no-retrigger behaviour.
- Add processor tests for CC7/CC10/CC11/CC71/CC74 mapping and invalid-controller
  safety.
- Build the VST3, AU, and Standalone targets; inspect each bundle to confirm
  the three files exist under `Contents/Resources/SoundFonts`.
- Run the rompler test binary, CTest, and pluginval when installed. Render the
  real editor with `ui_shot` to inspect the new wheels and legato control.

## Out of scope

- MIDI learn, user-configurable CC assignments, MPE per-note expression,
  tempo-synchronised vibrato, and cross-platform bundle-resource discovery.
