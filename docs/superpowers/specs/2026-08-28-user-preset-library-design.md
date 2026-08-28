# EON User Preset Library Design

**Date:** 2026-08-28
**Status:** Approved
**Scope:** User preset management, SoundFont portability, full-screen preset browser, favorites, and eight live quick slots

## Goal

Turn EON's current preset selection into a durable user preset system without coupling file management or browser UI logic to the audio engine. A preset must restore the audible instrument state, update every rotary control and its `init` UI state, survive missing SoundFont files, and remain safe to use in a DAW.

## Product Decisions

- EON remains preset-centered, with a small live-performance layer.
- The preset browser opens as a full editor overlay and temporarily covers the hardware panel.
- The `EON DS50` text is removed from the editor header.
- Factory presets are immutable. Editing one creates a dirty state and requires **Save As**.
- User presets can be overwritten, renamed, duplicated, deleted, imported, and exported.
- Classification combines built-in categories, user tags, and favorites.
- The first live-performance scope is eight quick slots plus MIDI Program Change mapping.
- Setlists, song ordering, preset morphing, cloud sync, and a preset marketplace are out of scope.

## Architecture

### `PresetLibrary`

Owns the in-memory catalog and exposes operations for listing, filtering, saving, duplicating, renaming, deleting, importing, exporting, favoriting, tagging, and assigning quick slots. It is the only component that coordinates library mutations. UI components consume its public model and never edit preset files directly.

### `PresetStorage`

Serializes versioned preset documents as JSON backed by JUCE `ValueTree` values. It separates the read-only factory location from the writable user location. Writes use a temporary file followed by replacement so an interrupted save does not leave a partially written preset.

### `SoundFontResolver`

Resolves a stored SoundFont identity to a local file. It checks the original path and known library locations, reports a missing state without deleting the preset, and supports explicit user relinking. Package import may install or reference an included SF2 only after validation.

### `PresetBrowserOverlay`

Renders search, category navigation, results, favorites, tags, management actions, missing-file recovery, and the quick-slot strip. It owns presentation state only. Closing the overlay restores the existing hardware panel unchanged.

### `RomplerProcessor` integration

The processor remains responsible for applying validated parameter, SF2, bank, and program state. Applying a preset uses host-notifying parameter updates on the message thread, then synchronizes all knob controls from their parameters and clears transient knob readouts so the controls visibly return to their `init` presentation.

No JSON parsing, directory scanning, file dialogs, blocking file I/O, allocation, UI calls, or locks may run in the audio callback. MIDI Program Change received during processing records only a lock-free pending quick-slot request. An `AsyncUpdater` or equivalent message-thread handoff resolves and loads the preset.

## Preset Data

Each preset document contains:

- schema version and stable UUID;
- display name, creation time, and modification time;
- source kind: factory or user;
- every sound, filter, envelope, dynamics, output, and effect parameter required to reproduce the state;
- SoundFont identity, original path, bank, and program;
- built-in category and user-defined tags;
- optional descriptive metadata required by future compatible schema versions.

Favorites and quick-slot assignments belong to a separate user library index rather than the immutable factory document. The index stores favorite preset UUIDs and quick slots `1–8`. This lets users personalize factory presets without modifying them.

The plug-in's normal APVTS host state remains self-contained for DAW project recall. A project must still reopen with its saved sound even if the global user library changes later.

## SoundFont Portability

Normal preset saves store the SoundFont identity and path without copying the SF2. If resolution fails, the browser displays `Missing SF2`, preserves the preset, and offers **Relink**.

Export offers two explicit formats:

1. preset-only export containing metadata and parameter state; or
2. portable package containing the preset and an optional SF2 copy.

The exporter warns that the user is responsible for redistribution rights before including an SF2. Import validates paths, file types, schema versions, and package contents before installing anything. An imported UUID collision creates a new UUID and preserves the existing preset.

## Browser Interaction

- Clicking the current preset name opens the full overlay.
- The left column contains All, Favorites, built-in categories, and User.
- The main column contains search results. Search matches name, category, tags, and SoundFont metadata.
- A single click loads a preset immediately when the current state is clean.
- Editing any stored parameter adds `*` to the current preset name.
- Selecting another preset while dirty opens a non-modal in-editor choice: **Save**, **Discard and Load**, or **Cancel**. It must not open a blocking native modal window.
- A context menu or `•••` action provides rename, duplicate, delete, import, and export where applicable.
- Factory entries never expose overwrite or delete.
- Missing-SF2 entries remain visible and offer Relink.
- Arrow keys move selection, Enter loads, and Escape closes the overlay.

## Quick Slots and MIDI

The overlay footer shows quick slots `1–8`. The current preset can be assigned through its action menu. Reassigning an occupied slot requires an inline confirmation.

MIDI Program Change values `0–7` select quick slots `1–8`. Values outside this range do nothing in this version. Missing or unassigned slots do not change the current sound and report a short non-modal status message. MIDI Bank Select, setlists, and arbitrary MIDI learn are deferred.

## Failure Handling

- Malformed user preset files are skipped, logged, and shown as recoverable invalid entries when their name can be read.
- Unsupported future schema versions remain untouched and display an upgrade-required message.
- Duplicate display names never silently overwrite a file. Save As proposes `Name (2)`, `Name (3)`, and so on.
- Failed atomic replacement leaves the existing preset intact and reports the save failure.
- A failed SF2 load leaves the current playable engine state unchanged.
- Import is staged and validated before files enter the user library.
- Deleting a user preset requires inline confirmation and never deletes its referenced external SF2.

## Performance and Threading

Directory scanning, JSON parsing, search-index construction, package validation, and SF2 resolution happen outside the audio callback. Catalog changes are published to the UI as complete snapshots. Search should remain responsive with at least 1,000 presets.

Preset application must preserve the existing safe loader-retirement rules. A new SoundFont is prepared away from the audio callback and published through the processor's established safe handoff. Parameter application and UI synchronization occur on the message thread.

## Validation

### Unit and component tests

- round-trip all preset fields and parameters;
- enforce factory overwrite and delete protection;
- save, rename, duplicate, and delete user presets;
- recover from display-name and UUID collisions;
- preserve existing files after interrupted or failed saves;
- detect, display, and relink missing SF2 files;
- reject malformed JSON, unsupported schemas, and unsafe package paths;
- import preset-only and portable packages without damaging existing data;
- exercise all Save, Discard and Load, and Cancel dirty-state paths;
- verify quick-slot persistence and Program Change `0–7` mapping;
- verify out-of-range, unassigned, and missing-SF2 quick-slot behavior;
- verify that preset application updates the 27 rotary parameters and resets their visible `init` state;
- measure search and overlay population with at least 1,000 generated presets.

### Integration gates

Validation remains separated by evidence type:

1. ROMpler unit and component tests;
2. Standalone and VST3 builds;
3. bundle installation and code-signature verification;
4. pluginval validation at the project's required strictness;
5. actual DAW recall, automation, Program Change, and rapid preset-switch testing;
6. real UI render inspection for removed product text, clipping, spacing, dirty prompts, missing-SF2 warnings, and knob synchronization.

Passing source tests alone does not imply that installation, signing, pluginval, DAW behavior, or UI rendering has passed.

## Delivery Boundaries

The implementation should be staged so the storage/model layer and tests land before the overlay, followed by processor application, quick slots, MIDI handoff, packaging, and final host/UI validation. Existing unrelated worktree changes must remain untouched. The `.superpowers/` visual brainstorming artifacts are not part of the product deliverable.
