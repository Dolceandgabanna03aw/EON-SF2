# Preset Knob Initialization and Legends Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reset every rotary sound parameter to its declared default when a SoundFont preset is selected, while preserving oversampling and keeping every knob's function name below it.

**Architecture:** `RomplerProcessor::selectPreset` remains the only preset-selection entry point and resets an explicit list of rotary APVTS parameters through the host-notifying path. The existing `Knob` component continues to own permanent legends and transient live values; only the wording of abbreviated labels changes.

**Tech Stack:** C++20, JUCE `AudioProcessorValueTreeState`, Catch2, CMake, JUCE headless `ui_shot`.

---

## File structure

- Modify: `plugins/rompler/Source/PluginProcessor.cpp` — reset defined rotary parameters after recording a selected bank/program.
- Modify: `plugins/rompler/test/test_plugin_smoke.cpp` — verify default restoration and preserved oversampling.
- Modify: `plugins/rompler/Source/PluginEditor.cpp` — make permanent rotary legends unambiguous.

The target source files already contain unrelated uncommitted work. Do not create a file-level commit for this plan until its hunks have been reviewed and isolated; keep the changes as a clean, tested working-tree patch.

### Task 1: Write the failing processor test

**Files:**

- Modify: `plugins/rompler/test/test_plugin_smoke.cpp`
- Test: `plugins/rompler/test/test_plugin_smoke.cpp`

- [ ] **Step 1: Add `#include <array>` with the standard includes, then add this test after `a new program starts with a conservative output trim`**

```cpp
TEST_CASE ("selecting a preset resets knobs but keeps oversampling", "[plugin][smoke]")
{
    aod::RomplerProcessor processor;
    auto& state = processor.getValueTreeState();
    const std::array<const char*, 27> knobIds {
        aod::ParamIDs::voiceDrive, aod::ParamIDs::voiceVelToDrive,
        aod::ParamIDs::voiceFilterOffset, aod::ParamIDs::busTapeDrive,
        aod::ParamIDs::busFold, aod::ParamIDs::busFilterCutoff,
        aod::ParamIDs::busFilterResonance, aod::ParamIDs::outTrim,
        aod::ParamIDs::outMix, aod::ParamIDs::fxChorusRate,
        aod::ParamIDs::fxChorusDepth, aod::ParamIDs::fxChorusMix,
        aod::ParamIDs::fxReverbRoom, aod::ParamIDs::fxReverbDamp,
        aod::ParamIDs::fxReverbMix, aod::ParamIDs::fxDelayMix,
        aod::ParamIDs::fxDelayFeedback, aod::ParamIDs::envAttack,
        aod::ParamIDs::envDecay, aod::ParamIDs::envSustain,
        aod::ParamIDs::envRelease, aod::ParamIDs::compThreshold,
        aod::ParamIDs::compRatio, aod::ParamIDs::compAttack,
        aod::ParamIDs::compRelease, aod::ParamIDs::compMakeup,
        aod::ParamIDs::compMix
    };
    for (const auto* id : knobIds)
    {
        auto* parameter = state.getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (0.13f);
    }
    auto* oversampling = state.getParameter (aod::ParamIDs::busOsFactor);
    REQUIRE (oversampling != nullptr);
    oversampling->setValueNotifyingHost (0.0f);

    processor.selectPreset (12, 34);

    for (const auto* id : knobIds)
    {
        const auto* parameter = state.getParameter (id);
        REQUIRE (std::abs (parameter->getValue() - parameter->getDefaultValue()) < 1.0e-5f);
    }
    REQUIRE (std::abs (oversampling->getValue()) < 1.0e-5f);
    const auto [bank, program] = processor.getCurrentBankProgram();
    REQUIRE (bank == 12);
    REQUIRE (program == 34);
}
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run: `cmake --preset plugin && cmake --build --preset plugin --target rompler_tests && ./build-plugin/plugins/rompler/test/rompler_tests "selecting a preset resets knobs but keeps oversampling"`

Expected: failure because `selectPreset` only records bank/program today.

- [ ] **Step 3: Check the focused diff without staging unrelated hunks**

Run: `git diff --check -- plugins/rompler/test/test_plugin_smoke.cpp`

### Task 2: Reset the precise rotary parameter set

**Files:**

- Modify: `plugins/rompler/Source/PluginProcessor.cpp:468-472`
- Test: `plugins/rompler/test/test_plugin_smoke.cpp`

- [ ] **Step 1: Add `#include <array>` directly in `PluginProcessor.cpp`, then change `RomplerProcessor::selectPreset` to this implementation**

```cpp
void RomplerProcessor::selectPreset (int bank, int program) noexcept
{
    currentBank_.store (bank, std::memory_order_relaxed);
    currentProgram_.store (program, std::memory_order_relaxed);

    static constexpr std::array<const char*, 27> rotaryParameterIds {
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
    for (const auto* id : rotaryParameterIds)
        if (auto* parameter = apvts_.getParameter (id))
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
}
```

`ParamIDs::busOsFactor`, `voiceCurve`, `voiceFilterRouting`, and `polyLimit` must not be in the list.

- [ ] **Step 2: Run the focused test and verify it passes**

Run: `cmake --build --preset plugin --target rompler_tests && ./build-plugin/plugins/rompler/test/rompler_tests "selecting a preset resets knobs but keeps oversampling"`

Expected: PASS.

- [ ] **Step 3: Run all rompler tests**

Run: `./build-plugin/plugins/rompler/test/rompler_tests`

Expected: all test cases in the rompler test executable pass.

- [ ] **Step 4: Check the implementation diff without staging unrelated hunks**

Run: `git diff --check -- plugins/rompler/Source/PluginProcessor.cpp plugins/rompler/test/test_plugin_smoke.cpp`

### Task 3: Clarify permanent rotary legends

**Files:**

- Modify: `plugins/rompler/Source/PluginEditor.cpp:1545-1575`
- Test: `plugins/rompler/tools/ui_shot.cpp` through rendered output

- [ ] **Step 1: Replace abbreviated labels in the existing `setKnobLabel` block**

```cpp
setKnobLabel (controls_[2], "VELOCITY > DRIVE");
setKnobLabel (controls_[29], "FILTER CUTOFF");
setKnobLabel (controls_[30], "RESONANCE");
setKnobLabel (controls_[9], "OUTPUT TRIM");
setKnobLabel (controls_[10], "OUTPUT MIX");
setKnobLabel (controls_[11], "CHORUS RATE");
setKnobLabel (controls_[12], "CHORUS DEPTH");
setKnobLabel (controls_[13], "CHORUS MIX");
setKnobLabel (controls_[14], "REVERB ROOM");
setKnobLabel (controls_[15], "REVERB DAMP");
setKnobLabel (controls_[16], "REVERB MIX");
setKnobLabel (controls_[17], "ATTACK");
setKnobLabel (controls_[18], "DECAY");
setKnobLabel (controls_[19], "SUSTAIN");
setKnobLabel (controls_[20], "RELEASE");
setKnobLabel (controls_[21], "THRESHOLD");
setKnobLabel (controls_[26], "COMP MIX");
```

Keep `name_` visible at rest and `value_` hidden at rest; do not change `Knob::setReadoutVisible`, `mouseDown`, `mouseUp`, or `mouseWheelMove`.

- [ ] **Step 2: Build and render the real editor**

Run: `cmake --build --preset dev --target ui_shot && mkdir -p outputs && ./build/plugins/rompler/ui_shot outputs/eon_ui_preset_init_labels.png`

Expected: the 1120x900 output shows every permanent function legend below its cap and no live numeric values at rest.

- [ ] **Step 3: Inspect `outputs/eon_ui_preset_init_labels.png`**

Expected: no label overlaps an adjacent knob.

- [ ] **Step 4: Check the legend diff without staging unrelated hunks**

Run: `git diff --check -- plugins/rompler/Source/PluginEditor.cpp`

### Task 4: Build the standalone artefact

**Files:**

- Modify: none
- Test: `build/plugins/rompler/EONDS50_artefacts/RelWithDebInfo/Standalone/EON-DS50.app`

- [ ] **Step 1: Build standalone and inspect whitespace**

Run: `cmake --build --preset dev --target EONDS50_Standalone && git diff --check -- plugins/rompler/Source/PluginProcessor.cpp plugins/rompler/Source/PluginEditor.cpp plugins/rompler/test/test_plugin_smoke.cpp`

Expected: standalone exits with status 0 and `git diff --check` emits no output.
