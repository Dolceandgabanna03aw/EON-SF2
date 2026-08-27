# Preset knob initialization and control legends

## Goal

When the user selects a SoundFont preset, reset every rotary knob to its
declared APVTS default value. Preserve the BUS oversampling selector exactly
as it was. Keep non-rotary switches and toggles unchanged.

Every rotary control must retain a clear, permanent function legend below the
knob. Its numeric value remains transient and appears only while the knob is
being adjusted.

## Scope

- `RomplerProcessor::selectPreset` becomes the single preset-selection reset
  point.
- Reset only parameters represented by the editor's `Knob` components.
- Do not reset `busOsFactor`, selector/switch parameters, or toggle parameters.
- Expand ambiguous abbreviated knob legends where layout permits, without
  changing the transient numeric readout behaviour.

## Behaviour

1. A bank/program selection is requested through the browser callback.
2. The processor updates the current bank/program pair.
3. The processor iterates the defined rotary parameter IDs and sets each to
   `getDefaultValue()` through the normal host-notifying parameter path.
4. The oversampling selector is intentionally omitted from that list, so its
   current 1x/2x/4x/8x selection survives the preset switch.
5. The editor receives ordinary APVTS change notifications and redraws its
   knob positions. No editor-specific reset logic is required.

## Error handling and compatibility

- Missing parameter IDs are skipped defensively; this prevents a partial
  registration issue from crashing a preset selection.
- The existing state save/restore flow remains unchanged: restoring a host
  state restores the saved values rather than applying a preset-selection
  reset.
- The reset emits host notifications, so automation and host parameter state
  stay coherent.

## Tests

Add a processor smoke test that first moves representative knobs and the
oversampling selector away from their defaults, selects a preset, and verifies:

- Every selected knob parameter equals its default normalized value.
- The oversampling selector remains unchanged.
- The selected bank/program pair is updated.

The existing UI render test is used to confirm permanent function legends stay
below every knob and transient values are not introduced at rest.
