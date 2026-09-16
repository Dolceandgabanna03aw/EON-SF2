# Aoi YUME Bank Names Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rename the three bundled SoundFonts and present concise, related bank names without breaking restoration of sessions that stored the old bundled filenames.

**Architecture:** Keep absolute SoundFont paths as the processor's persistence identity. Derive display names from the active path, and canonicalise the three legacy bundled basenames only when state restoration falls back to the packaged SoundFonts directory.

**Tech Stack:** C++20, JUCE, CMake, Catch2, macOS VST3/AU/Standalone bundles.

---

### Task 1: Specify display and legacy-name behaviour

**Files:**
- Modify: `plugins/rompler/test/test_plugin_smoke.cpp`
- Modify: `plugins/rompler/Source/PluginProcessor.h`

- [x] Add a test proving a loaded bank displays only its filename stem.
- [x] Add table-driven tests for the three legacy-to-canonical bundled filename mappings.
- [x] Run the focused smoke test and confirm the new assertions fail before implementation.

### Task 2: Implement bank naming and compatibility

**Files:**
- Modify: `plugins/rompler/Source/PluginProcessor.h`
- Modify: `plugins/rompler/Source/PluginProcessor.cpp`

- [x] Add `canonicalBundledSoundFontFileName()` for the three old filenames.
- [x] Make `getBankName()` and `getLoadedFileName()` return the filename stem while `getBankPath()` continues returning the absolute persistence path.
- [x] Apply canonicalisation only to packaged-directory fallback during state restore.
- [x] Run the focused smoke test and confirm it passes.

### Task 3: Rename and package the SF2 assets

**Files:**
- Rename: `testdata/sf2/Sonic_Mania_-_Korg_M1_Legacy_Soundfont.sf2` to `testdata/sf2/Crystal Legacy.sf2`
- Rename: `testdata/sf2/Live HQ Natural SoundFont GM.sf2` to `testdata/sf2/Natural Stage.sf2`
- Rename: `testdata/sf2/SGM-v2.01-NicePianosGuitarsBass-V1.2.sf2` to `testdata/sf2/Studio Essentials.sf2`
- Modify: `plugins/rompler/CMakeLists.txt`

- [x] Rename all three source assets without changing their bytes.
- [x] Update CMake source paths and the deterministic default filename.
- [x] Build VST3, AU, Standalone, and tests.
- [x] Verify each generated bundle contains exactly the three canonical filenames.

### Task 4: Full verification

**Files:**
- Verify: `plugins/rompler/test/test_plugin_smoke.cpp`

- [x] Run the full CTest suite with failure output enabled.
- [x] Run `git diff --check`.
- [x] Confirm old filenames remain only in compatibility mapping or historical documentation.
- [x] Checkpoint completion evidence.
