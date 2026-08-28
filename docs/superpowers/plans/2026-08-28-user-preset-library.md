# EON User Preset Library Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a durable user preset library to the EON rompler: versioned JSON preset documents, atomic saves, SoundFont relinking, a full-editor preset browser overlay, favourites and tags, eight quick slots driven by MIDI Program Change, and preset application that visibly returns every rotary control to its `init` presentation.

**Architecture:** Four new message-thread-only modules sit beside the existing plug-in. `PresetStorage` serialises and atomically writes versioned JSON documents. `SoundFontResolver` maps a stored SoundFont identity onto a real file and supports explicit relinking. `PresetLibrary` owns the catalogue and every mutation, plus a separate user index holding favourites, tags and quick slots. `PresetBrowserOverlay` is presentation only and covers the hardware panel while open. `RomplerProcessor` keeps sole responsibility for applying validated parameter, SoundFont, bank and program state, and gains a lock-free pending-request slot so a MIDI Program Change never touches the filesystem on the audio thread.

**Tech Stack:** C++20, JUCE 8.0.14 (`juce_core` for `JSON`, `TemporaryFile`, `ZipFile`; `juce_audio_processors` for APVTS), Catch2 v3.7.1, CMake presets, JUCE headless `ui_shot`.

---

## Ground rules

Read these before the first edit. Violating any of them silently breaks the build or the user's uncommitted work.

- **The worktree is dirty with unrelated user work.** `plugins/rompler/Source/*.{h,cpp}`, `plugins/rompler/test/*.cpp`, both `CMakeLists.txt` files and several untracked files carry uncommitted changes that are not part of this plan. Never run `git reset`, `git checkout --`, `git stash` or `git add -A`. Stage only the exact paths a step names.
- **Build with the `plugin` preset only.** The rompler is guarded by `X10_BUILD_PLUGINS`, so `cmake --preset dev` does not compile it at all and `build/` still contains a stale `rompler_tests_NOT_BUILT` placeholder. Every command in this plan uses `build-plugin/`.
- **Warnings are errors.** `EONDS50`, `ui_shot` and `rompler_tests` all compile with `-Wall -Wextra -Wpedantic -Werror -Wshadow`. An unused parameter or a shadowed local fails the build.
- **Catch2 links without a main.** `rompler_tests` links `Catch2::Catch2`, not `Catch2::Catch2WithMain`. The single `main()` lives in `plugins/rompler/test/test_plugin_smoke.cpp` and installs a `juce::ScopedJuceInitialiser_GUI`. Never add a `main()` to a new test file.
- **New sources need two registrations.** A new `Source/*.cpp` goes into the `target_sources(EONDS50 PRIVATE ...)` list in `plugins/rompler/CMakeLists.txt`; a new `test/*.cpp` goes into the `add_executable(rompler_tests ...)` list in `plugins/rompler/test/CMakeLists.txt`. `catch_discover_tests` then registers its cases automatically.
- **No audio-thread file work.** JSON parsing, directory scanning, dialogs, allocation, locks and UI calls stay off `processBlock`. The audio thread may only read and write plain atomics.
- **Tests must never write into the real user library.** `PresetLibrary` takes its root directory as a constructor argument. Production code passes `PresetStorage::userLibraryDirectory()`; tests pass a scratch directory under the system temp folder.

## Parameter storage contract

The plug-in declares 32 parameters. This plan splits them into three fixed sets and stores only the first two in a preset:

| Set | Count | Members | Stored in a preset |
| --- | --- | --- | --- |
| `ParamSets::rotary` | 27 | every `AudioParameterFloat` bound to a `Knob` | yes |
| `ParamSets::presetChoices` | 3 | `voice.curve`, `voice.filterRouting`, `voice.legato` | yes |
| `ParamSets::engineOnly` | 2 | `bus.osFactor`, `poly.limit` | no |

`bus.osFactor` and `poly.limit` are engine budget settings, not sound design. Recalling them from a preset would renegotiate plug-in latency and PDC on every single-click preset load, and it would contradict the existing `selectPreset` contract, which already preserves oversampling. Choice parameters are stored by **label text**, not index, so reordering a `juce::StringArray` in `Parameters.h` can never silently repoint an already-saved preset.

## File structure

Create:

- `plugins/rompler/Source/PresetModel.h` - `PresetSource`, `PresetParameterValue`, `PresetDocument`.
- `plugins/rompler/Source/PresetStorage.h` / `.cpp` - JSON encode/decode, atomic write, library locations.
- `plugins/rompler/Source/SoundFontResolver.h` / `.cpp` - path resolution and relink support.
- `plugins/rompler/Source/PresetLibrary.h` / `.cpp` - catalogue, CRUD, index, search, import/export.
- `plugins/rompler/Source/PresetBrowserOverlay.h` / `.cpp` - the full-editor browser component.
- `plugins/rompler/presets/factory/*.eonpreset` - bundled read-only factory documents.
- `plugins/rompler/test/test_preset_storage.cpp`, `test_preset_library.cpp`, `test_preset_apply.cpp`.

Modify:

- `plugins/rompler/Source/Parameters.h` - add the canonical `ParamSets` lists.
- `plugins/rompler/Source/PluginProcessor.h` / `.cpp` - capture/apply a preset, pending quick-slot request, `AsyncUpdater` handoff, absolute bank paths.
- `plugins/rompler/Source/PluginEditor.h` / `.cpp` - remove the product text, add the knob `init` presentation, host the overlay.
- `plugins/rompler/CMakeLists.txt` - register new sources and bundle the factory presets.
- `plugins/rompler/test/CMakeLists.txt` - register new test files.

---

### Task 1: Make the parameter sets a single source of truth

Today the 27 rotary identifiers are duplicated in `PluginProcessor.cpp:612` and implied again by the `Knob` construction list in `PluginEditor.cpp:1677`. A preset that stores the wrong set is silently wrong, so the list moves into `Parameters.h` first.

**Files:**

- Modify: `plugins/rompler/Source/Parameters.h`
- Modify: `plugins/rompler/Source/PluginProcessor.cpp`
- Test: `plugins/rompler/test/test_plugin_smoke.cpp`

- [ ] **Step 1: Add the failing coverage test to `test_plugin_smoke.cpp`, after the existing `selecting a preset resets knobs but keeps oversampling` case**

```cpp
TEST_CASE ("parameter sets cover every declared parameter exactly once", "[plugin][params]")
{
    aod::RomplerProcessor processor;
    auto& state = processor.getValueTreeState();

    juce::StringArray declared;
    for (auto* parameter : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            declared.add (withId->paramID);

    juce::StringArray covered;
    for (const auto* id : aod::ParamSets::rotary)
        covered.add (id);
    for (const auto* id : aod::ParamSets::presetChoices)
        covered.add (id);
    for (const auto* id : aod::ParamSets::engineOnly)
        covered.add (id);

    REQUIRE (aod::ParamSets::rotary.size() == 27);
    REQUIRE (aod::ParamSets::presetChoices.size() == 3);
    REQUIRE (aod::ParamSets::engineOnly.size() == 2);
    REQUIRE (covered.size() == declared.size());

    covered.sort (false);
    declared.sort (false);
    REQUIRE (covered == declared);

    for (const auto& id : covered)
        REQUIRE (state.getParameter (id) != nullptr);
}
```

- [ ] **Step 2: Confirm it fails to compile**

Run: `cmake --preset plugin && cmake --build --preset plugin --target rompler_tests`

Expected: compile error, `no member named 'ParamSets' in namespace 'aod'`.

- [ ] **Step 3: Add `ParamSets` to `Parameters.h`, immediately after the closing brace of `namespace Choices`**

Also add `#include <array>` next to the existing `#include <juce_audio_processors/juce_audio_processors.h>`.

```cpp
/**
    Canonical parameter groupings. Anything that has to iterate a subset of the
    plug-in's parameters - preset capture, preset application, the knob reset in
    selectPreset() - reads these lists instead of restating them, so the three
    call sites cannot drift apart.
*/
namespace ParamSets
{
/** Every float parameter that is drawn as a rotary control. */
inline constexpr std::array<const char*, 27> rotary {
    ParamIDs::voiceDrive, ParamIDs::voiceVelToDrive, ParamIDs::voiceFilterOffset,
    ParamIDs::busTapeDrive, ParamIDs::busFold, ParamIDs::busFilterCutoff,
    ParamIDs::busFilterResonance, ParamIDs::outTrim, ParamIDs::outMix,
    ParamIDs::fxChorusRate, ParamIDs::fxChorusDepth, ParamIDs::fxChorusMix,
    ParamIDs::fxReverbRoom, ParamIDs::fxReverbDamp, ParamIDs::fxReverbMix,
    ParamIDs::fxDelayMix, ParamIDs::fxDelayFeedback, ParamIDs::envAttack,
    ParamIDs::envDecay, ParamIDs::envSustain, ParamIDs::envRelease,
    ParamIDs::compThreshold, ParamIDs::compRatio, ParamIDs::compAttack,
    ParamIDs::compRelease, ParamIDs::compMakeup, ParamIDs::compMix
};

/** Choice parameters that describe the sound and therefore belong in a preset. */
inline constexpr std::array<const char*, 3> presetChoices {
    ParamIDs::voiceCurve, ParamIDs::voiceFilterRouting, ParamIDs::voiceLegato
};

/**
    Engine budget settings. Deliberately excluded from presets: recalling them
    would renegotiate latency/PDC on every preset click, and selectPreset()
    already contracts to preserve oversampling.
*/
inline constexpr std::array<const char*, 2> engineOnly {
    ParamIDs::busOsFactor, ParamIDs::polyLimit
};
} // namespace ParamSets
```

- [ ] **Step 4: Replace the local array in `RomplerProcessor::selectPreset`**

Delete the `static constexpr std::array<const char*, 27> rotaryParameterIds { ... };` block and change the loop to read the shared list:

```cpp
    for (const auto* id : ParamSets::rotary)
        if (auto* parameter = apvts_.getParameter (id))
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
```

- [ ] **Step 5: Confirm both tests pass**

Run: `cmake --build --preset plugin --target rompler_tests && ./build-plugin/plugins/rompler/test/rompler_tests "[params]" "[smoke]"`

Expected: all cases pass, including the pre-existing knob reset test.

- [ ] **Step 6: Commit only these three files**

Run: `git add -- plugins/rompler/Source/Parameters.h plugins/rompler/Source/PluginProcessor.cpp plugins/rompler/test/test_plugin_smoke.cpp && git diff --cached --check && git commit -m "refactor(rompler): centralise parameter set definitions"`

Note: `Parameters.h` and `PluginProcessor.cpp` already contain unrelated uncommitted work. Review `git diff --cached` before committing and, if unrelated hunks are present, commit with `git add -p` instead so only this task's hunks are staged.

---

### Task 2: Remove the `EON-DS50` product text from the header

The design removes the product text; the painted `AOI YUME` engraving stays and moves left to close the gap. `brandSub_` is already an invisible child, so both brand labels go.

**Files:**

- Modify: `plugins/rompler/Source/PluginEditor.h`
- Modify: `plugins/rompler/Source/PluginEditor.cpp`
- Test: `plugins/rompler/tools/ui_shot.cpp` through a rendered PNG

- [ ] **Step 1: Delete both brand members from `RomplerEditor` in `PluginEditor.h`**

Remove these two lines from the private member block:

```cpp
    DepthLabel brandTitle_;
    DepthLabel brandSub_;
```

- [ ] **Step 2: Delete their construction in `RomplerEditor::RomplerEditor`**

Remove the whole block that starts with `addAndMakeVisible (brandTitle_);` and ends with the `brandSub_.setFont (...)` call, including the two-sentence comment above `addChildComponent (brandSub_);`. The constructor body should continue straight from the initialiser list to `addAndMakeVisible (voiceBox_);`.

- [ ] **Step 3: Reclaim the header row in `RomplerEditor::resized`**

Replace these four lines:

```cpp
    auto top = b.removeFromTop (58);
    const auto brandColumn = top.removeFromLeft (220);
    brandTitle_.setBounds (brandColumn.withTop (top.getY() + 1).withHeight (30));
    brandSub_.setBounds (brandColumn.withTop (top.getY() + 29).withHeight (25));
```

with:

```cpp
    // The header is paint-only for now: the engraved signature is drawn in
    // paint(), and Task 13 places the preset-name button in this strip.
    const auto header = b.removeFromTop (58);
    juce::ignoreUnused (header);
```

- [ ] **Step 4: Move the engraved signature left in `RomplerEditor::paint`**

The signature rectangle was inset by 238 px purely to clear the deleted 220 px brand column. Change:

```cpp
    const auto signature = topPanel.withTrimmedLeft (238.0f)
```

to:

```cpp
    const auto signature = topPanel.withTrimmedLeft (18.0f)
```

Leave `withTrimmedRight (420.0f)` and both `drawFittedText` passes untouched.

- [ ] **Step 5: Render the real editor and inspect it**

Run: `cmake --build --preset plugin --target ui_shot && mkdir -p outputs && ./build-plugin/plugins/rompler/ui_shot outputs/eon_ui_no_product_text.png`

Expected: a 1120x900 PNG. Open it and confirm: no `EON-DS50` text anywhere, the `AOI YUME` engraving sits near the left edge of the metal header, nothing overlaps it, and no empty 220 px hole remains. Record which part of the header strip is still visually free - Task 13 places the preset-name button there.

- [ ] **Step 6: Confirm nothing else referenced the deleted members**

Run: `rg -n 'brandTitle_|brandSub_|EON-DS50' plugins/rompler --glob '!*.bak'`

Expected: no matches.

- [ ] **Step 7: Run the full test executable and commit**

Run: `cmake --build --preset plugin --target rompler_tests && ./build-plugin/plugins/rompler/test/rompler_tests`

Expected: all cases pass.

Then: `git add -- plugins/rompler/Source/PluginEditor.h plugins/rompler/Source/PluginEditor.cpp && git diff --cached --check && git commit -m "feat(ui): remove product text from the editor header"`

---

### Task 3: Preset document model and JSON round trip

**Files:**

- Create: `plugins/rompler/Source/PresetModel.h`
- Create: `plugins/rompler/Source/PresetStorage.h`, `plugins/rompler/Source/PresetStorage.cpp`
- Create: `plugins/rompler/test/test_preset_storage.cpp`
- Modify: `plugins/rompler/CMakeLists.txt`, `plugins/rompler/test/CMakeLists.txt`

- [ ] **Step 1: Write `plugins/rompler/test/test_preset_storage.cpp` first**

```cpp
#include <catch2/catch_test_macros.hpp>

#include "PresetStorage.h"

namespace
{
aod::PresetDocument makeDocument()
{
    aod::PresetDocument document;
    document.uuid = "6f0f0c8e-0000-4000-8000-000000000001";
    document.name = "Glass Bell";
    document.category = "Keys";
    document.tags = juce::StringArray { "bright", "clean" };
    document.source = aod::PresetSource::user;
    document.createdAtMs = 1700000000000LL;
    document.modifiedAtMs = 1700000000500LL;
    document.soundFontName = "Sonic_Mania_-_Korg_M1_Legacy_Soundfont.sf2";
    document.soundFontPath = "/tmp/eon/Sonic_Mania_-_Korg_M1_Legacy_Soundfont.sf2";
    document.bank = 3;
    document.program = 17;
    document.parameters.push_back ({ "voice.drive", false, 0.625f, {} });
    document.parameters.push_back ({ "voice.curve", true, 0.0f, "Tube" });
    return document;
}
} // namespace

TEST_CASE ("a preset document survives a JSON round trip", "[preset][storage]")
{
    const auto original = makeDocument();
    const auto parsed = aod::PresetStorage::fromJson (aod::PresetStorage::toJson (original));

    REQUIRE (parsed.status == aod::PresetStorage::ParseStatus::ok);

    const auto& restored = parsed.document;
    REQUIRE (restored.schemaVersion == aod::PresetDocument::currentSchemaVersion);
    REQUIRE (restored.uuid == original.uuid);
    REQUIRE (restored.name == original.name);
    REQUIRE (restored.category == original.category);
    REQUIRE (restored.tags == original.tags);
    REQUIRE (restored.source == aod::PresetSource::user);
    REQUIRE (restored.createdAtMs == original.createdAtMs);
    REQUIRE (restored.modifiedAtMs == original.modifiedAtMs);
    REQUIRE (restored.soundFontName == original.soundFontName);
    REQUIRE (restored.soundFontPath == original.soundFontPath);
    REQUIRE (restored.bank == 3);
    REQUIRE (restored.program == 17);
    REQUIRE (restored.parameters.size() == 2);

    const auto* drive = restored.find ("voice.drive");
    REQUIRE (drive != nullptr);
    REQUIRE_FALSE (drive->isChoice);
    REQUIRE (drive->value == 0.625f);

    const auto* curve = restored.find ("voice.curve");
    REQUIRE (curve != nullptr);
    REQUIRE (curve->isChoice);
    REQUIRE (curve->text == "Tube");
}

TEST_CASE ("a factory document keeps its source across a round trip", "[preset][storage]")
{
    auto original = makeDocument();
    original.source = aod::PresetSource::factory;

    const auto parsed = aod::PresetStorage::fromJson (aod::PresetStorage::toJson (original));
    REQUIRE (parsed.status == aod::PresetStorage::ParseStatus::ok);
    REQUIRE (parsed.document.source == aod::PresetSource::factory);
}
```

- [ ] **Step 2: Register the new test file, then confirm it fails**

Add `test_preset_storage.cpp` as a new line in the `add_executable(rompler_tests ...)` list in `plugins/rompler/test/CMakeLists.txt`.

Run: `cmake --build --preset plugin --target rompler_tests`

Expected: compile error, `'PresetStorage.h' file not found`.

#### Continuation of Task 3

`PresetModel` and `PresetStorage` are deliberately separate from APVTS. The model is a portable document, while APVTS remains the live host-automation state. This keeps factory files, user files, and package exports independent from host XML.

**Files:**

- Create `plugins/rompler/Source/PresetModel.h`.
- Create `plugins/rompler/Source/PresetStorage.h` and `PresetStorage.cpp`.
- Modify `plugins/rompler/CMakeLists.txt`.
- Modify `plugins/rompler/test/CMakeLists.txt`.
- Create `plugins/rompler/test/test_preset_storage.cpp` (from Step 1).

- [ ] **Step 3: Add the model header exactly as specified by the API contract**

Keep `PresetParameterValue` aggregate initialization order `{ id, isChoice, value, text }`. Store normalized float values for rotary parameters and visible labels for choices. Use `PresetSource::factory` to mark immutable bundled documents.

- [ ] **Step 4: Implement deterministic JSON serialization**

Emit every top-level field in the contract, omit `text` only for float values, and serialize `source` as `user` or `factory`. Preserve parameter order so generated files are stable and easy to diff.

- [ ] **Step 5: Implement strict parsing and schema handling**

Reject missing required fields, wrong JSON types, non-finite/out-of-range normalized values, duplicate parameter IDs, and unknown source values as `malformed`. Return `unsupportedSchema` for versions newer than `currentSchemaVersion`, retaining recoverable name/version fields.

- [ ] **Step 6: Add round-trip and rejection tests, then build**

Run `cmake --build --preset plugin --target rompler_tests` and `./build-plugin/plugins/rompler/test/rompler_tests "[preset][storage]"`. Confirm malformed and future-schema cases fail without crashing.

### Task 4: Atomic storage and library locations

Preset writes must survive interruption and must never partially replace a valid file. The user and factory roots are distinct so tests can use temporary directories and factory content remains read-only.

**Files:** `plugins/rompler/Source/PresetStorage.cpp`, `plugins/rompler/test/test_preset_storage.cpp`.

- [ ] **Step 1: Resolve platform library directories**

Return an application-support `EON SF2/Presets` user directory and the read-only bundle `Resources/Presets` factory directory. `userLibraryDirectory()` must create no directories as a side effect.

- [ ] **Step 2: Implement extension and filename sanitizing**

Return `.eonpreset` and `.eonpack`. Strip path separators, control characters, reserved names, and repeated whitespace; provide a non-empty fallback such as `Untitled`.

- [ ] **Step 3: Implement write-then-replace semantics**

Write JSON to a sibling temporary file, flush it, then replace the destination using JUCE file operations. Remove only the temporary file on failure and report `temporaryFileFailed` or `replaceFailed` precisely.

- [ ] **Step 4: Test interruption-safe behavior**

Use a temporary directory, verify the destination remains byte-for-byte unchanged when the temporary write or replacement is forced to fail, and run the storage tag again.

### Task 5: SoundFont resolution and relinking

Preset documents remember both a display name and the original absolute path. Resolution first trusts an existing path, then searches known directories by filename, and finally uses an explicit relink remembered for the session.

**Files:** `plugins/rompler/Source/SoundFontResolver.h`, `SoundFontResolver.cpp`, `plugins/rompler/test/test_soundfont_resolver.cpp`, `plugins/rompler/CMakeLists.txt`, `plugins/rompler/test/CMakeLists.txt`.

- [ ] **Step 1: Implement ordered resolution**

Return `resolved` for an existing stored path, `relocated` for a matching file in a search directory or relink map, and `missing` otherwise. Never mutate or delete the preset when missing.

- [ ] **Step 2: Implement default search directories**

Include bundle `Resources/SoundFonts` and the user SoundFonts folder, de-duplicated and filtered to existing directories.

- [ ] **Step 3: Add relink and ambiguity tests**

Remember a user-selected file by filename, verify subsequent documents resolve as `relocated`, and verify a missing file produces `missing` with no write.

### Task 6: Library catalogue and 1000-entry search

`PresetLibrary` owns scanning, parsing, filtering, and index metadata on the message thread. Search must remain responsive with a thousand documents and must surface malformed or missing-SF2 rows instead of silently dropping them.

**Files:** `plugins/rompler/Source/PresetLibrary.h`, `PresetLibrary.cpp`, `plugins/rompler/test/test_preset_library.cpp`, build lists.

- [ ] **Step 1: Scan user and factory roots**

Recursively enumerate only `.eonpreset` files, parse each file, mark `invalid` or `upgradeRequired` when appropriate, and sort factory entries before user entries by case-insensitive name.

- [ ] **Step 2: Implement text/category/favourite/user filters**

Search name, category, tags, and SoundFont name with case-folded matching. Empty query fields impose no constraint. Return stable pointers into `entries()` until the next rescan.

- [ ] **Step 3: Add a 1000-document performance test**

Generate 1000 temporary documents, rescan, search with combined filters, and assert completion under 100 ms in a release-like test build while checking exact result membership.

### Task 7: CRUD, immutability, and name collisions

Factory presets are immutable; user mutations must be atomic and must update the in-memory catalogue only after the file operation succeeds. Names are human-facing, while UUIDs are identity.

**Files:** `plugins/rompler/Source/PresetLibrary.cpp`, `plugins/rompler/test/test_preset_library.cpp`.

- [ ] **Step 1: Implement saveAs, overwrite, rename, duplicate, remove**

Generate UUIDs, update timestamps, sanitize filenames, and use `proposeUniqueName()` (`Bell`, `Bell (2)`, …). Reject overwrite/rename/remove of factory entries with `readOnly`.

- [ ] **Step 2: Implement favourite, tags, and quick-slot metadata**

Persist these fields only in `library-index.json`, allowing factory rows to be favourited without modifying factory files.

- [ ] **Step 3: Verify failure atomicity and callbacks**

Every successful mutation and rescan invokes `onCatalogueChanged` once; failed operations leave both disk and memory untouched. Add tests for duplicate names, unknown UUIDs, and factory mutation attempts.

### Task 8: User index and quick-slot persistence

The index is separate from preset documents so favourites and slots survive factory updates and document exports. Invalid UUIDs in the index are ignored during load and removed on the next successful save.

**Files:** `plugins/rompler/Source/PresetLibrary.cpp`, `plugins/rompler/test/test_preset_library.cpp`.

- [ ] **Step 1: Implement versioned index load/save**

Read `library-index.json` with the contract schema, default missing arrays safely, clamp slots to eight entries, and atomically rewrite after metadata changes.

- [ ] **Step 2: Test restart persistence and stale entries**

Construct a second library over the same temporary root and verify favourites/quick slots persist. Delete a referenced preset, rescan, and verify the stale UUID is cleared without touching other entries.

### Task 9: Capture and apply processor presets

Preset application must reproduce the audible state in a load-bearing order: SoundFont and bank first, then program selection, then all stored parameters. This avoids `switchBank()` overwriting restored values.

**Files:** `plugins/rompler/Source/PluginProcessor.h`, `PluginProcessor.cpp`, `plugins/rompler/test/test_processor_presets.cpp`.

- [ ] **Step 1: Add capturePreset()**

Capture all 27 rotary and 3 choice values, current SoundFont absolute path/name, bank, and program. Do not serialize `bus.osFactor` or `poly.limit`.

- [ ] **Step 2: Fix bankNames_ to retain absolute paths**

Store `file.getFullPathName()` in `loadSoundFont()` and expose the path needed by capture. Preserve existing state XML compatibility when reading older filename-only state.

- [ ] **Step 3: Add applyPreset() with explicit status**

Resolve/load the SoundFont, call `switchBank`, call `selectPreset`, then restore rotary and choice parameters by ID/label. Return `soundFontMissing` or `soundFontLoadFailed` without partially claiming success.

- [ ] **Step 4: Test order and excluded engine settings**

Set deliberately non-default values, apply a document, and assert all stored values match while osFactor/polyLimit remain unchanged. Include a bank whose first program differs from the target to prove ordering.

### Task 10: Knob init-state feedback

Preset changes must update the visual init state immediately, not only the underlying APVTS value. The knob uses the existing accent paint path and reports exact default equality through a small queryable API.

**Files:** `plugins/rompler/Source/PluginEditor.h`, `PluginEditor.cpp`, `plugins/rompler/test/test_knob_init_state.cpp`.

- [ ] **Step 1: Add atInit_ and refreshInitState()**

Refresh after parameter synchronization, async listener delivery, and `mouseUp()`. Compare the parameter's normalized value with its default using the same tolerance as the control display.

- [ ] **Step 2: Use init state in paint()**

Render the init accent when `atInit_` is true and the modified accent otherwise. Keep hover/hot state orthogonal so interaction feedback is not lost.

- [ ] **Step 3: Test preset-switch UI synchronization**

Drive a knob away from default, invoke the editor's preset callback, pump JUCE messages, and assert `isAtInitState()` becomes true for restored defaults and false for non-default values.

### Task 11: MIDI program-change quick slots

Program changes 0–7 are a real-time input and must not touch the message-thread library or load files from the audio callback. A lock-free pending slot is handed to `AsyncUpdater` and consumed on the message thread.

**Files:** `plugins/rompler/Source/PluginProcessor.h`, `PluginProcessor.cpp`, `PluginEditor.cpp`, `plugins/rompler/test/test_quick_slots.cpp`.

- [ ] **Step 1: Make RomplerProcessor an AsyncUpdater**

Add `requestQuickSlot(int) noexcept`, clamp/ignore values outside 1..8, atomically store the latest request, and trigger an async update without allocation or locks on the audio thread.

- [ ] **Step 2: Route PC 0–7 from processBlock()**

Translate the incoming program-change number to slot 1..8, retain the existing current-program bookkeeping, and never call `applyPreset()` directly from `processBlock()`.

- [ ] **Step 3: Deliver onQuickSlotRequested on the message thread**

In `handleAsyncUpdate()`, consume the pending slot and invoke the editor callback once. The editor resolves the UUID, SoundFont, and applies the preset on the message thread.

- [ ] **Step 4: Add real-time safety tests**

Test all eight mappings, invalid values, and coalescing. Review the callback path to ensure no filesystem, mutex, allocation, or UI access occurs before async delivery.

### Task 12: Preset browser overlay

The browser is an opaque, reusable overlay rather than a blocking native dialog. Every keyboard affordance also has an on-screen control because the current host configuration disables keyboard focus requests.

**Files:** Create `plugins/rompler/Source/PresetBrowserOverlay.h`, `PresetBrowserOverlay.cpp`; modify editor/build lists; add `test_preset_browser_overlay.cpp`.

- [ ] **Step 1: Implement model binding and selection callbacks**

Render search/category/favourite controls and rows from `PresetLibrary::search()`. Clicking a row emits `onLoadRequested(uuid)` without applying the preset inside the component.

- [ ] **Step 2: Implement dirty-choice flow**

When dirty, show Save, Discard, and Cancel buttons and emit `onDirtyChoice`. Factory rows remain visibly read-only; missing SoundFonts show a relink affordance.

- [ ] **Step 3: Implement geometry and input interception**

Paint a dimmed backdrop, intercept mouse clicks, bring the overlay to front, and provide visible Up/Down/Enter/Escape buttons that mirror keyboard actions.

- [ ] **Step 4: Remove timer stomping**

Change `RomplerEditor::timerCallback()` so it does not overwrite overlay selection while the overlay is open; refresh the browser only after catalogue changes or explicit close.

### Task 13: Header preset name and dirty indicator

The main editor must always show which preset is active and whether live controls differ from it. This is separate from the browser overlay and remains visible during normal editing.

**Files:** `plugins/rompler/Source/PluginEditor.h`, `PluginEditor.cpp`, `plugins/rompler/test/test_editor_preset_header.cpp`.

- [ ] **Step 1: Add current-name and dirty labels**

Display the loaded document name, use `*` or a compact LED for dirty state, and keep the label readable at the existing 1120x900 size.

- [ ] **Step 2: Track dirty state from parameter and bank changes**

Compare a fresh `capturePreset()` against the loaded document on the message thread. Bank/program/SF2 changes count as dirty; engine-only osFactor/polyLimit changes do not alter the preset document.

- [ ] **Step 3: Test header transitions**

Load, edit, save, and switch presets while asserting label text and dirty state after message pumping.

### Task 14: Import and export packages

Portable exchange must support a single `.eonpreset` and a `.eonpack` zip containing the document and optionally its SoundFont. Imports are copied into the user root and never overwrite factory content.

**Files:** `plugins/rompler/Source/PresetLibrary.cpp`, `plugins/rompler/Source/PresetStorage.cpp`, tests, build lists.

- [ ] **Step 1: Implement preset-only export/import**

Export a canonical JSON file, parse imported files before copying, assign a fresh UUID on collision, and use the unique-name proposal for display names.

- [ ] **Step 2: Implement package manifest and safe extraction inside PresetLibrary/PresetStorage**

Write a manifest plus one document and optional SoundFont. Reject absolute paths, `..` traversal, duplicate entries, oversized archives, and malformed manifests before extraction.

- [ ] **Step 3: Add round-trip and failure tests**

Export/import with and without SoundFont, verify resolver behavior, and assert invalid packages leave the user library unchanged.

### Task 15: Factory preset bundle

Factory presets must ship with the plugin and be discoverable on first launch without requiring a writable install location. The bundle contents are test fixtures and product resources, not host state.

**Files:** `plugins/rompler/Resources/Presets/*.eonpreset`, `plugins/rompler/CMakeLists.txt`, `plugins/rompler/test/test_factory_presets.cpp`.

- [ ] **Step 1: Create a minimal curated factory set**

Add representative clean, tube, transformer, and ambient documents using only the contracted parameter IDs and bundled SoundFont names.

- [ ] **Step 2: Copy resources into the plugin bundle**

Add a POST_BUILD copy step beside the existing SoundFonts copy and verify paths for macOS VST3/AU layouts.

- [ ] **Step 3: Test read-only discovery**

Instantiate `PresetLibrary` with the bundle root, assert factory source and non-empty metadata, and verify overwrite/remove return `readOnly`.

### Task 16: Integration gates and release verification

The feature is complete only when source, build, plugin validation, installation, and actual UI rendering are checked separately. Unit tests alone do not prove host behavior.

**Files:** `plugins/rompler/test/*`, `plugins/rompler/CMakeLists.txt`, documentation as needed.

- [ ] **Step 1: Run source and unit validation**

Run `cmake --preset plugin`, `cmake --build --preset plugin --target rompler_tests`, then `./build-plugin/plugins/rompler/test/rompler_tests "[preset]"` and the full test binary. Record failures without masking them.

- [ ] **Step 2: Run static and packaging checks**

Run `git diff --check`, confirm `-Wall -Wextra -Wpedantic -Werror`, verify every new source is registered exactly once, and inspect the assembled bundle's Presets/SoundFonts resources.

- [ ] **Step 3: Validate plugin and rendered UI separately**

Run the registered `pluginval_vst3_strictness10` CTest, install/codesign the bundle, capture `./build-plugin/plugins/rompler/ui_shot /tmp/eon-ui.png`, and inspect that image for the removed product text, header preset name, dirty indicator, knob init accent, and overlay clipping.

- [ ] **Step 4: Perform host smoke testing**

In a real host, load a factory preset, edit and save a user preset, switch presets, relink a missing SoundFont, press visible overlay controls, and send MIDI PCs 0–7. Report DAW behavior independently from source/build evidence.

- [ ] **Step 5: Update the implementation handoff**

List any remaining limitations (especially keyboard focus in hosts, missing model resources, or unverified AU behavior) and hand the approved plan to the selected execution workflow.
