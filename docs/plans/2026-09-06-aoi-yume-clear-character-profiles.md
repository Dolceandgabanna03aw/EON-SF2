# Aoi YUME Clear and Character Profiles Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Ship a clear modern default and a textured alternate factory profile
without changing Aoi YUME’s plugin identity or hiding audible state from its
existing controls.

**Architecture:** The APVTS keeps the same parameter IDs and ranges. Clear
defaults are declared in `Parameters.h`; two JSON factory documents reference
the bundled `Crystal Legacy.sf2` and set those existing parameters. CMake copies
the documents into each plugin format’s `Contents/Resources/Presets` directory.

**Tech Stack:** JUCE APVTS, Catch2, CMake presets, VST3 pluginval.

---

### Task 1: Lock the clear default in a failing processor test

**Files:**
- Modify: `plugins/rompler/test/test_plugin_smoke.cpp`
- Modify: `plugins/rompler/Source/Parameters.h`

**Step 1:** Add a test that constructs `RomplerProcessor` and asserts the
defaults for `fx.chorusRate`, `fx.chorusDepth`, `fx.chorusMix`,
`fx.reverbRoom`, `fx.reverbDamp`, `fx.reverbMix`, `bus.osFactor`, and
`out.trim` equal the Clear table in the design.

**Step 2:** Build `rompler_tests` and run only that test. It must fail because
the prior FX defaults are still 1 Hz / 30% / 25% and 40% / 50% / 20%.

**Step 3:** Change only those existing parameter defaults. Do not change their
IDs, ranges, skew, or automation conversion.

**Step 4:** Rebuild and rerun the focused test, then the full rompler test
executable.

### Task 2: Package two visible factory profile documents

**Files:**
- Create: `plugins/rompler/Resources/Presets/Aoi Clear.eonpreset`
- Create: `plugins/rompler/Resources/Presets/Aoi Character.eonpreset`
- Modify: `plugins/rompler/CMakeLists.txt`
- Modify: `plugins/rompler/test/test_preset_library.cpp`

**Step 1:** Add a failing test that reads both resource documents through
`PresetStorage::readFile`, verifies `PresetSource::factory`, profile name,
category, soundfont name, and the profile-defining parameter values.

**Step 2:** Run the focused test. It must fail because neither document exists.

**Step 3:** Add schema-valid documents with stable UUIDs, factory source,
`Crystal Legacy.sf2`, bank/program zero, and values from the design table.
Include every captured rotary/choice parameter required by `PresetStorage`.

**Step 4:** Extend the existing CMake post-build resource step to copy the
`Resources/Presets` directory into all VST3/AU/Standalone bundles before the
final ad-hoc signing step.

**Step 5:** Rerun the focused library test, build `EONDS50_VST3`, and assert
the two documents exist in the bundle’s `Contents/Resources/Presets` directory.

### Task 3: Execute full safety and format verification

**Files:**
- Verify only.

**Step 1:** Run `git diff --check`.

**Step 2:** Run `cmake --build --preset plugin --target rompler_tests -j2` and
`./build-plugin/plugins/rompler/test/rompler_tests --reporter compact`.

**Step 3:** Build the VST3 target once, verify its bundle signature, then run
`ctest --preset plugin --output-on-failure` exactly once; do not run concurrent
builds against `build-plugin`.

**Step 4:** Report source/build/test/pluginval evidence separately from the
remaining DAW load and listening A/B verification.
