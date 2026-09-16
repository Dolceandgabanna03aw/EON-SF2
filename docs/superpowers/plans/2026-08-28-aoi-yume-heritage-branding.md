# Aoi YUME Heritage Branding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Rename the plugin display product to Aoi YUME, set the vendor to EON LAB, and add a recessed Aoi YUME engraving to the Heritage Rack UI.

**Architecture:** Keep JUCE product identity and binary compatibility codes unchanged while updating display metadata. Extend the existing custom JUCE editor paint path so the engraving is rendered as a hardware-style embossed mark without changing DSP or parameter behavior.

**Tech Stack:** JUCE/C++, CMake, existing rompler editor and smoke/build tests.

---

### Task 1: Update plugin identity metadata

**Files:**
- Modify: `plugins/rompler/CMakeLists.txt`
- Test: existing CMake configure/build and plugin smoke metadata checks

- [ ] Change `PRODUCT_NAME` to `Aoi YUME` and add/update the JUCE manufacturer display field to `EON LAB`; leave `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`, and `BUNDLE_ID` unchanged.
- [ ] Reconfigure and build the plugin targets to verify the metadata is accepted by JUCE/CMake.
- [ ] Inspect generated VST3 metadata or run the existing smoke test and confirm product name/vendor are `Aoi YUME`/`EON LAB`.

### Task 2: Add Heritage Rack engraving

**Files:**
- Modify: `plugins/rompler/Source/PluginEditor.cpp`
- Modify: `plugins/rompler/Source/PluginEditor.h` only if a helper declaration is required
- Test: `ui_shot` render and visual inspection

- [ ] Add a small recessed metal-engraving draw helper in the existing editor paint path, using the current palette and font setup.
- [ ] Render `Aoi YUME` once in the hardware faceplate (not as a duplicate interactive label), with a dark inset shadow and restrained highlight so it reads as engraved metal.
- [ ] Build `ui_shot`, render the editor, and inspect that the engraving is visible without clipping or overlapping controls.

### Task 3: Regression verification

**Files:**
- No source changes expected.
- Test: plugin unit/smoke tests, strict plugin validation where available, and `git diff --check`

- [ ] Run the focused rompler tests and existing plugin smoke test.
- [ ] Run the configured plugin build and `ui_shot` target.
- [ ] Run `git diff --check` and report any unavailable host/plugin validation separately from passing source/build checks.
