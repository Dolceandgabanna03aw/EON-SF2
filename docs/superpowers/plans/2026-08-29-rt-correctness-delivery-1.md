# EON SF2 Real-Time Correctness Delivery 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make MIDI timing, UI note delivery, host handoff, voice allocation, 128-note polyphony, and SoundFont replacement correct and bounded without blocking or notifying the host from the audio callback.

**Architecture:** Split the processor into a sample-range renderer plus a MIDI event dispatcher so audio is rendered up to each event offset before that event mutates engine state. Use fixed-capacity SPSC queues and coalesced atomic mailboxes at thread boundaries, add deterministic ownership metadata to the fixed 128-voice pool, and keep immutable SoundFont banks alive with audio-thread reference counters reclaimed only on the message thread.

**Tech Stack:** C++20, JUCE 8.0.14, Catch2, CMake `plugin` preset, atomics, fixed-capacity containers, ASan/UBSan.

---

## Working-tree rules

The checkout already contains user-owned, uncommitted MIDI, DSP, preset, browser, and UI work. Every task must begin with `git status --short` and must stage only the files listed for that task. Do not reset, restore, replace, or broadly format an existing file. Review `git diff -- <files>` before every commit so the delivery adapts to the working tree instead of discarding it.

The rompler is built only by the `plugin` preset. `rompler_tests` uses the single Catch2/GUI main in `plugins/rompler/test/test_plugin_smoke.cpp`; new test files must not define another `main()`.

## File map

- Create `plugins/rompler/Source/RealtimeQueue.h`: bounded SPSC storage for message-thread UI notes.
- Modify `plugins/rompler/Source/PluginProcessor.h`: realtime event types, coalesced control mailbox, bank publication state, and range-rendering declarations.
- Modify `plugins/rompler/Source/PluginProcessor.cpp`: sample-offset dispatch, range rendering, async host mirroring, latency handoff, and bounded bank reclamation.
- Modify `plugins/rompler/Source/Sampler.h`: fixed 128-voice capacity, voice ownership metadata, envelope/audibility accessors, and bank lifetime token.
- Modify `plugins/rompler/Source/Sampler.cpp`: deterministic stealing, stale-map cleanup, bank reference accounting, and range rendering support.
- Modify `plugins/rompler/Source/SF2Loader.h`: immutable bank lifetime counter and message-thread reclamation query.
- Modify `plugins/rompler/test/CMakeLists.txt`: register the two focused test files.
- Create `plugins/rompler/test/test_realtime_queue.cpp`: bounded FIFO and overflow policy tests.
- Create `plugins/rompler/test/test_rt_correctness.cpp`: sample offsets, controller handoff, latency, 128 voices, stealing, and bank lifetime regression tests.
- Modify `plugins/rompler/test/test_plugin_smoke.cpp`: retain whole-processor regressions that need the existing JUCE main and SF2 fixture.

### Task 1: Replace the UI-note mutex with a bounded SPSC queue

**Files:**
- Create: `plugins/rompler/Source/RealtimeQueue.h`
- Create: `plugins/rompler/test/test_realtime_queue.cpp`
- Modify: `plugins/rompler/test/CMakeLists.txt:1-15`
- Modify: `plugins/rompler/Source/PluginProcessor.h:3-8,133-138,182-220`
- Modify: `plugins/rompler/Source/PluginProcessor.cpp:104-126,753-764`

- [ ] **Step 1: Register and write the failing queue tests**

Add `test_realtime_queue.cpp` to the existing `rompler_tests` source list and create:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "RealtimeQueue.h"

namespace
{
struct Event { int note; bool on; int velocity; };
}

TEST_CASE ("SPSC queue preserves FIFO order", "[rt][queue]")
{
    aod::SpscQueue<Event, 4> queue;
    REQUIRE (queue.tryPush ({ 60, true, 100 }));
    REQUIRE (queue.tryPush ({ 60, false, 0 }));
    Event event {};
    REQUIRE (queue.tryPop (event));
    REQUIRE (event.note == 60);
    REQUIRE (event.on);
    REQUIRE (queue.tryPop (event));
    REQUIRE_FALSE (event.on);
    REQUIRE_FALSE (queue.tryPop (event));
}

TEST_CASE ("SPSC queue drops newest event when full", "[rt][queue]")
{
    aod::SpscQueue<int, 3> queue;
    REQUIRE (queue.tryPush (1));
    REQUIRE (queue.tryPush (2));
    REQUIRE_FALSE (queue.tryPush (3));
    int value = 0;
    REQUIRE (queue.tryPop (value)); REQUIRE (value == 1);
    REQUIRE (queue.tryPop (value)); REQUIRE (value == 2);
}
```

- [ ] **Step 2: Build to verify RED**

Run: `cmake --preset plugin && cmake --build --preset plugin --target rompler_tests`

Expected: compilation fails because `RealtimeQueue.h` and `aod::SpscQueue` do not exist.

- [ ] **Step 3: Implement the fixed-capacity queue**

Create `RealtimeQueue.h` with the complete queue implementation:

```cpp
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace aod
{
template <typename T, std::size_t Capacity>
class SpscQueue
{
    static_assert (Capacity >= 2);
    static_assert (std::is_trivially_copyable_v<T>);

public:
    [[nodiscard]] bool tryPush (const T& value) noexcept
    {
        const auto write = write_.load (std::memory_order_relaxed);
        const auto next = increment (write);
        if (next == read_.load (std::memory_order_acquire))
            return false;
        storage_[write] = value;
        write_.store (next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool tryPop (T& value) noexcept
    {
        const auto read = read_.load (std::memory_order_relaxed);
        if (read == write_.load (std::memory_order_acquire))
            return false;
        value = storage_[read];
        read_.store (increment (read), std::memory_order_release);
        return true;
    }

private:
    [[nodiscard]] static constexpr std::size_t increment (std::size_t index) noexcept
    {
        return (index + 1) % Capacity;
    }

    std::array<T, Capacity> storage_ {};
    alignas (64) std::atomic<std::size_t> write_ { 0 };
    alignas (64) std::atomic<std::size_t> read_ { 0 };
};
} // namespace aod
```

In `PluginProcessor.h`, replace `<queue>`, `<mutex>`, and the tuple queue with:

```cpp
#include "RealtimeQueue.h"

struct UiNoteEvent { int note; int velocity; bool on; };
SpscQueue<UiNoteEvent, maxQueuedNotes + 1> noteQueue_;
std::atomic<bool> noteQueueOverflowed_ { false };
```

In `postNote()`, clamp note/velocity, call `tryPush`, and set `noteQueueOverflowed_` when the queue is full. In `processBlock()`, repeatedly `tryPop()` into a stack `UiNoteEvent`; do not copy, move, lock, allocate, or recreate a queue.

- [ ] **Step 4: Verify GREEN and the no-mutex source invariant**

Run:

```bash
cmake --build --preset plugin --target rompler_tests
./build-plugin/plugins/rompler/test/rompler_tests "[rt][queue]"
! rg -n "noteQueueMutex|std::queue|std::lock_guard" plugins/rompler/Source/PluginProcessor.*
```

Expected: two queue tests pass and the final search returns no matches.

- [ ] **Step 5: Commit only Task 1 files**

```bash
git add plugins/rompler/Source/RealtimeQueue.h plugins/rompler/Source/PluginProcessor.h plugins/rompler/Source/PluginProcessor.cpp plugins/rompler/test/test_realtime_queue.cpp plugins/rompler/test/CMakeLists.txt
git diff --cached --check
git commit -m "fix(rompler): make UI note delivery lock free"
```

### Task 2: Render MIDI at exact event offsets

**Files:**
- Create: `plugins/rompler/test/test_rt_correctness.cpp`
- Modify: `plugins/rompler/test/CMakeLists.txt:1-16`
- Modify: `plugins/rompler/Source/PluginProcessor.h:182-245`
- Modify: `plugins/rompler/Source/PluginProcessor.cpp:74-377`

- [ ] **Step 1: Write failing same-block timing tests**

Add `test_rt_correctness.cpp` to the test target. Include `<catch2/catch_approx.hpp>` as well as the test-macro header. Reuse the existing `X10_SF2_CROSSCHECK_TESTDATA` environment-variable lookup, and define one complete fixture in that file:

```cpp
namespace
{
constexpr int blockSize = 512;

juce::File requireSf2Fixture()
{
    const auto path = juce::SystemStats::getEnvironmentVariable (
        "X10_SF2_CROSSCHECK_TESTDATA", {});
    if (path.isEmpty())
        SKIP ("X10_SF2_CROSSCHECK_TESTDATA is not set");

    juce::Array<juce::File> files;
    juce::File (path).findChildFiles (files, juce::File::findFiles, true, "*.sf2");
    if (files.isEmpty())
        SKIP ("No SF2 fixture exists under X10_SF2_CROSSCHECK_TESTDATA");
    return files.getFirst();
}

struct PreparedProcessor
{
    PreparedProcessor()
        : fixtureFile (requireSf2Fixture()), state (processor.getValueTreeState())
    {
        processor.setPlayConfigDetails (0, 2, 48000.0, blockSize);
        processor.prepareToPlay (48000.0, blockSize);
        REQUIRE (processor.loadSoundFont (fixtureFile));
        const auto selection = processor.firstPlayablePresetAndKeyForTesting();
        REQUIRE (selection.has_value());
        bank = selection->bank;
        program = selection->program;
        key = selection->key;
        processor.selectPreset (bank, program);
        setNormalized (aod::ParamIDs::fxChorusMix, 0.0f);
        setNormalized (aod::ParamIDs::fxReverbMix, 0.0f);
        setNormalized (aod::ParamIDs::fxDelayMix, 0.0f);
        setNormalized (aod::ParamIDs::compMix, 0.0f);
        buffer.clear();
    }

    ~PreparedProcessor() { processor.releaseResources(); }

    void setNormalized (const juce::String& id, float normalized)
    {
        auto* parameter = state.getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (normalized);
    }

    void setEnvelopeReleaseToMinimum() { setNormalized (aod::ParamIDs::envRelease, 0.0f); }

    void setOversamplingChoice (int index)
    {
        auto* parameter = state.getParameter (aod::ParamIDs::oversampling);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
    }

    void startFixtureNote()
    {
        midi.addEvent (juce::MidiMessage::noteOn (1, key, (juce::uint8) 100), 0);
        processor.processBlock (buffer, midi);
        midi.clear();
        buffer.clear();
    }

    void releaseAndDrainVoice()
    {
        midi.addEvent (juce::MidiMessage::noteOff (1, key), 0);
        processor.processBlock (buffer, midi);
        midi.clear();
        for (int block = 0; block < 512 && processor.activeVoiceCountForTesting() != 0; ++block)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
        REQUIRE (processor.activeVoiceCountForTesting() == 0);
    }

    RomplerProcessor processor;
    juce::AudioBuffer<float> buffer { 2, blockSize };
    juce::MidiBuffer midi;
    juce::File fixtureFile;
    juce::AudioProcessorValueTreeState& state;
    int bank = 0;
    int program = 0;
    int key = 60;
};
}
```

Add read-only processor test helpers `firstPlayablePresetAndKeyForTesting()` and `activeVoiceCountForTesting()`. The former returns `std::optional<PlayableSelection>` with concrete `bank`, `program`, and `key` fields after searching the loaded loader; the latter delegates to the voice pool. These helpers are never called from `processBlock()` and this file must not introduce a second Catch2 main.

Then add these cases:

```cpp
TEST_CASE ("note on begins at its MIDI sample offset", "[rt][midi]")
{
    PreparedProcessor fixture;
    constexpr int offset = 127;
    fixture.midi.addEvent (juce::MidiMessage::noteOn (1, fixture.key, (juce::uint8) 100), offset);
    fixture.processor.processBlock (fixture.buffer, fixture.midi);
    REQUIRE (fixture.buffer.getMagnitude (0, offset) == Catch::Approx (0.0f).margin (1.0e-7f));
    REQUIRE (fixture.buffer.getMagnitude (offset, fixture.buffer.getNumSamples() - offset) > 0.0f);
}

TEST_CASE ("same-block note off renders only the requested interval", "[rt][midi]")
{
    PreparedProcessor fixture;
    fixture.setEnvelopeReleaseToMinimum();
    fixture.midi.addEvent (juce::MidiMessage::noteOn (1, fixture.key, (juce::uint8) 100), 64);
    fixture.midi.addEvent (juce::MidiMessage::noteOff (1, fixture.key), 192);
    fixture.processor.processBlock (fixture.buffer, fixture.midi);
    REQUIRE (fixture.buffer.getMagnitude (0, 64) == Catch::Approx (0.0f).margin (1.0e-7f));
    REQUIRE (fixture.buffer.getMagnitude (64, 128) > 0.0f);
    REQUIRE (fixture.buffer.getMagnitude (256, fixture.buffer.getNumSamples() - 256) < 1.0e-5f);
}
```

- [ ] **Step 2: Run the focused tests to verify RED**

Run:

```bash
cmake --build --preset plugin --target rompler_tests
./build-plugin/plugins/rompler/test/rompler_tests "[rt][midi]"
```

Expected: the pre-event silence assertion fails because the current processor applies every MIDI event before rendering the full buffer.

- [ ] **Step 3: Extract event dispatch and sample-range rendering**

Declare in `PluginProcessor.h`:

```cpp
void dispatchMidiMessage (const juce::MidiMessage&, SF2Loader&) noexcept;
void dispatchUiNote (const UiNoteEvent&, SF2Loader&) noexcept;
void renderRange (juce::AudioBuffer<float>&, int startSample, int numSamples) noexcept;
void syncBlockParameters() noexcept;
```

Restructure `processBlock()` to this exact control flow:

```cpp
buffer.clear();
syncBlockParameters();
auto* loader = activeLoader_.load (std::memory_order_acquire);
if (voicePool_ == nullptr || loader == nullptr)
    return;

UiNoteEvent uiEvent {};
while (noteQueue_.tryPop (uiEvent))
    dispatchUiNote (uiEvent, *loader); // block-boundary position zero

const int blockSamples = buffer.getNumSamples();
int cursor = 0;
for (const auto metadata : midiMessages)
{
    const int position = juce::jlimit (0, blockSamples, metadata.samplePosition);
    if (position > cursor)
        renderRange (buffer, cursor, position - cursor);
    dispatchMidiMessage (metadata.getMessage(), *loader);
    cursor = position;
}
if (cursor < blockSamples)
    renderRange (buffer, cursor, blockSamples - cursor);
lastPeak_.store (buffer.getMagnitude (0, blockSamples), std::memory_order_relaxed);
```

`dispatchUiNote()` clamps the queued note and velocity, resolves the current bank/program sample through the supplied loader, and calls the same voice-pool start/stop functions as MIDI note messages. Move the voice, CC gain/pan, bus, dynamics, FX, and output stages into `renderRange()`. Build a non-owning JUCE buffer view over `channelPointer + startSample` and `numSamples`; every processor receives only that range. Preserve event iteration order for equal sample positions and handle zero-length ranges without invoking DSP.

- [ ] **Step 4: Verify GREEN plus existing MIDI behavior**

Run:

```bash
cmake --build --preset plugin --target rompler_tests
./build-plugin/plugins/rompler/test/rompler_tests "[rt][midi]"
./build-plugin/plugins/rompler/test/rompler_tests "[plugin][midi]"
```

Expected: offset tests pass; sustain, legato, all-notes-off, pitch-wheel, and existing processor MIDI tests remain green.

- [ ] **Step 5: Commit only Task 2 files**

```bash
git add plugins/rompler/Source/PluginProcessor.h plugins/rompler/Source/PluginProcessor.cpp plugins/rompler/test/test_rt_correctness.cpp plugins/rompler/test/CMakeLists.txt
git diff --cached --check
git commit -m "fix(rompler): render MIDI at sample offsets"
```

### Task 3: Move host notifications and control mirroring off the audio callback

**Files:**
- Modify: `plugins/rompler/Source/PluginProcessor.h:28,182-245`
- Modify: `plugins/rompler/Source/PluginProcessor.cpp:74-245,738-780`
- Modify: `plugins/rompler/test/test_rt_correctness.cpp`

- [ ] **Step 1: Write failing controller and latency handoff tests**

Add processor diagnostic getters that only load atomics: `getRealtimeFilterOffsetForTesting()`, `getRealtimeBusCutoffForTesting()`, and `getPendingLatencyForTesting()`. Then add:

```cpp
TEST_CASE ("CC71 and CC74 update DSP before APVTS mirroring", "[rt][handoff]")
{
    PreparedProcessor fixture;
    auto* offset = fixture.state.getParameter (aod::ParamIDs::voiceFilterOffset);
    auto* cutoff = fixture.state.getParameter (aod::ParamIDs::busFilterCutoff);
    const float oldOffset = offset->getValue();
    const float oldCutoff = cutoff->getValue();
    fixture.midi.addEvent (juce::MidiMessage::controllerEvent (1, 71, 5), 32);
    fixture.midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 120), 64);
    fixture.processor.processBlock (fixture.buffer, fixture.midi);
    REQUIRE (fixture.processor.getRealtimeFilterOffsetForTesting() != Catch::Approx (oldOffset));
    REQUIRE (fixture.processor.getRealtimeBusCutoffForTesting() != Catch::Approx (oldCutoff));
    REQUIRE (offset->getValue() == Catch::Approx (oldOffset));
    REQUIRE (cutoff->getValue() == Catch::Approx (oldCutoff));
}

TEST_CASE ("oversampling change queues latency notification", "[rt][handoff]")
{
    PreparedProcessor fixture;
    const int before = fixture.processor.getLatencySamples();
    fixture.setOversamplingChoice (3);
    fixture.processor.processBlock (fixture.buffer, fixture.midi);
    REQUIRE (fixture.processor.getPendingLatencyForTesting() >= 0);
    REQUIRE (fixture.processor.getLatencySamples() == before);
}
```

- [ ] **Step 2: Verify RED**

Run: `cmake --build --preset plugin --target rompler_tests && ./build-plugin/plugins/rompler/test/rompler_tests "[rt][handoff]"`

Expected: tests fail because CC71/74 call `setValueNotifyingHost()` synchronously and oversampling calls `setLatencySamples()` inside `processBlock()`.

- [ ] **Step 3: Implement a coalesced atomic mailbox**

Define `enum AsyncFlag : std::uint32_t` with `mirrorFilterOffset = 1u << 0`, `mirrorBusCutoff = 1u << 1`, `mirrorLatency = 1u << 2`, and `quickSlot = 1u << 3`, backed by `std::atomic<std::uint32_t> pendingAsyncFlags_ { 0 }`. Store the newest normalized CC values in `std::atomic<float>` members and requested latency in `std::atomic<int> pendingLatencySamples_ { -1 }`. `dispatchMidiMessage()` updates the realtime DSP member immediately, stores the mirror value, calls `pendingAsyncFlags_.fetch_or (flag, std::memory_order_release)`, and calls `triggerAsyncUpdate()`; it never touches an APVTS parameter object.

At block start, synchronize APVTS values into the realtime members only when the parameter value differs from its cached last-mirrored value. This prevents an old APVTS value from overwriting a CC change while its async mirror is pending.

Change oversampling selection in `syncBlockParameters()` to select the already-prepared bus path and queue the requested latency. Move `setLatencySamples()` into `handleAsyncUpdate()`. Handle all flags from one acquired exchange:

```cpp
void RomplerProcessor::handleAsyncUpdate()
{
    const auto flags = pendingAsyncFlags_.exchange (0, std::memory_order_acq_rel);
    if ((flags & mirrorFilterOffset) != 0)
        mirrorNormalizedParameter (ParamIDs::voiceFilterOffset, pendingFilterOffsetNormalized_.load());
    if ((flags & mirrorBusCutoff) != 0)
        mirrorNormalizedParameter (ParamIDs::busFilterCutoff, pendingBusCutoffNormalized_.load());
    if ((flags & mirrorLatency) != 0)
        setLatencySamples (pendingLatencySamples_.load());
    if ((flags & quickSlot) != 0)
        dispatchPendingQuickSlot();
}
```

`mirrorNormalizedParameter()` is message-thread-only and calls `setValueNotifyingHost()` once with the latest value. Program Change `0..7` sets only the quick-slot mailbox; it must not select a SoundFont, resolve a preset, or notify a parameter in `processBlock()`.

- [ ] **Step 4: Verify GREEN and scan the callback body**

Run:

```bash
cmake --build --preset plugin --target rompler_tests
./build-plugin/plugins/rompler/test/rompler_tests "[rt][handoff]"
sed -n '/void RomplerProcessor::processBlock/,/^}/p' plugins/rompler/Source/PluginProcessor.cpp | rg "setValueNotifyingHost|setLatencySamples|loadSoundFont|applyPreset"
```

Expected: focused tests pass and the source scan returns no matches.

- [ ] **Step 5: Commit Task 3 files**

```bash
git add plugins/rompler/Source/PluginProcessor.h plugins/rompler/Source/PluginProcessor.cpp plugins/rompler/test/test_rt_correctness.cpp
git diff --cached --check
git commit -m "fix(rompler): defer host notifications from audio"
```

### Task 4: Make voice stealing deterministic and prepare all 128 voices

**Files:**
- Modify: `plugins/rompler/Source/Sampler.h:33-153`
- Modify: `plugins/rompler/Source/Sampler.cpp:25-413`
- Modify: `plugins/rompler/Source/PluginProcessor.cpp:25-45`
- Modify: `plugins/rompler/test/test_dsp_behaviors.cpp`
- Modify: `plugins/rompler/test/test_rt_correctness.cpp`

- [ ] **Step 1: Write failing ownership, priority, and capacity tests**

Add read-only diagnostics `VoicePool::activeVoiceCount()`, `VoicePool::voiceIndexForNote(int)`, and `VoicePool::preparedCapacity()`. Add tests that:

```cpp
TEST_CASE ("prepared pool honors the public 128 voice limit", "[rt][voice]")
{
    aod::VoicePool pool;
    REQUIRE (pool.preparedCapacity() == 128);
    pool.setPolyphony (128);
    for (int note = 0; note < 128; ++note)
        pool.start (&sample, note, 1.0f);
    REQUIRE (pool.activeVoiceCount() == 128);
}

TEST_CASE ("stealing clears the victim note mapping", "[rt][voice]")
{
    aod::VoicePool pool (2);
    pool.start (&sample, 60, 1.0f);
    pool.start (&sample, 62, 1.0f);
    pool.start (&sample, 64, 1.0f);
    REQUIRE (pool.voiceIndexForNote (64) >= 0);
    REQUIRE ((pool.voiceIndexForNote (60) == -1 || pool.voiceIndexForNote (62) == -1));
}
```

Add a release-priority case that renders one voice into Release, starts a new note, and verifies the releasing mapping is cleared while a held voice remains mapped. Add a stable age tie-break case with equal envelope levels.

- [ ] **Step 2: Verify RED**

Run: `cmake --build --preset plugin --target rompler_tests && ./build-plugin/plugins/rompler/test/rompler_tests "[rt][voice]"`

Expected: the capacity test reports 32 and stale ownership/priority tests fail.

- [ ] **Step 3: Implement fixed capacity and audibility metadata**

Set `VoicePool::maxVoices = 128` and make the default constructor allocate exactly `maxVoices`. Add to `Voice`:

```cpp
[[nodiscard]] float currentEnvelopeLevel() const noexcept { return currentEnvelopeLevel_; }
[[nodiscard]] std::uint64_t startSequence() const noexcept { return startSequence_; }
void setStartSequence (std::uint64_t sequence) noexcept { startSequence_ = sequence; }
float currentEnvelopeLevel_ = 0.0f;
std::uint64_t startSequence_ = 0;
```

Update `currentEnvelopeLevel_` on every ADSR tick and reset it when inactive. Increment a `VoicePool::nextStartSequence_` for every fresh start. Replace `findFreeVoice()` with an index-returning selector ordered by:

1. first inactive slot;
2. releasing slot with the lowest envelope, then oldest sequence;
3. active slot with the lowest envelope, then oldest sequence, avoiding the legato lead and physically held sustained voices when an equally quiet unprotected candidate exists.

Before reusing an index, read the victim note and clear `noteToVoice_[victimNote]` only when it still points to that index. Then publish the new mapping. Preserve retrigger, sustain, legato, and all-notes-off behavior.

- [ ] **Step 4: Verify GREEN and the public range**

Run:

```bash
cmake --build --preset plugin --target rompler_tests
./build-plugin/plugins/rompler/test/rompler_tests "[rt][voice]"
./build-plugin/plugins/rompler/test/rompler_tests "[dsp][voice]"
```

Expected: 128 capacity, deterministic victim selection, and all legacy voice tests pass.

- [ ] **Step 5: Commit Task 4 files**

```bash
git add plugins/rompler/Source/Sampler.h plugins/rompler/Source/Sampler.cpp plugins/rompler/Source/PluginProcessor.cpp plugins/rompler/test/test_dsp_behaviors.cpp plugins/rompler/test/test_rt_correctness.cpp
git diff --cached --check
git commit -m "fix(rompler): harden voice allocation and polyphony"
```

### Task 5: Bound SoundFont lifetime without audio-thread destruction

**Files:**
- Modify: `plugins/rompler/Source/SF2Loader.h:16-39`
- Modify: `plugins/rompler/Source/PluginProcessor.h:79-102,182-245`
- Modify: `plugins/rompler/Source/PluginProcessor.cpp:47-62,441-523,738-751`
- Modify: `plugins/rompler/Source/Sampler.h:33-153`
- Modify: `plugins/rompler/Source/Sampler.cpp:25-413`
- Modify: `plugins/rompler/test/test_rt_correctness.cpp`

- [ ] **Step 1: Write failing bank lifetime tests**

Add message-thread diagnostics `retiredBankCountForTesting()` and `collectRetiredBanksForTesting()`. Add a regression that loads the fixture as bank A, starts a note from A, reloads that same fixture to create a distinct bank B, verifies A remains retired while its voice is active, releases and renders the old voice to silence, collects, and verifies the retired count returns to zero. Repeat same-file replacement 16 times with no active voices and require the retired count is zero after every collection; no second corpus file is required.

```cpp
TEST_CASE ("retired banks are reclaimed after their voices finish", "[rt][bank]")
{
    PreparedProcessor fixture;
    fixture.startFixtureNote();
    REQUIRE (fixture.processor.loadSoundFont (fixture.fixtureFile));
    REQUIRE (fixture.processor.retiredBankCountForTesting() == 1);
    fixture.releaseAndDrainVoice();
    fixture.processor.collectRetiredBanksForTesting();
    REQUIRE (fixture.processor.retiredBankCountForTesting() == 0);
}

TEST_CASE ("inactive-bank replacement remains bounded", "[rt][bank]")
{
    PreparedProcessor fixture;
    for (int replacement = 0; replacement < 16; ++replacement)
    {
        REQUIRE (fixture.processor.loadSoundFont (fixture.fixtureFile));
        fixture.processor.collectRetiredBanksForTesting();
        REQUIRE (fixture.processor.retiredBankCountForTesting() == 0);
    }
}
```

- [ ] **Step 2: Verify RED**

Run: `cmake --build --preset plugin --target rompler_tests && ./build-plugin/plugins/rompler/test/rompler_tests "[rt][bank]"`

Expected: retired loader storage grows because the current `retiredLoaders_` vector is never reclaimed before processor destruction.

- [ ] **Step 3: Add an immutable bank token with audio references**

Introduce `SoundFontBank` in `SF2Loader.h`, after the complete `SF2Loader` definition, so token users see a complete loader type. It is owned by the message thread:

```cpp
struct SoundFontBank
{
    explicit SoundFontBank (int sampleRate) : loader (sampleRate) {}
    SF2Loader loader;
    std::atomic<std::uint32_t> activeVoiceRefs { 0 };
};
```

The processor owns `std::array<std::unique_ptr<SoundFontBank>, maxBanks>`, publishes `std::atomic<SoundFontBank*> activeBank_`, and keeps replaced banks in `retiredBanks_`. `Voice::start()` accepts `SoundFontBank*`, increments `activeVoiceRefs`, and stores the pointer alongside the `Sample*`. Before retrigger/steal and when a voice becomes inactive, `Voice::releaseBankToken()` decrements exactly once and clears both pointers.

`collectRetiredBanks()` runs only from `handleAsyncUpdate()`, `loadSoundFont()`, `removeBank()`, `releaseResources()`, or the explicit test hook. It erases retired banks whose `activeVoiceRefs` is zero and that are not the currently published bank. No `unique_ptr`, `shared_ptr`, loader, sample vector, or file object is destroyed in `processBlock()` or `Voice::render()`.

On failed replacement load, leave the current bank, bank name, current bank/program, and active voices unchanged. Publish the new bank before moving the previous owner to retirement, using release/acquire ordering.

- [ ] **Step 4: Verify GREEN under sanitizer**

Run:

```bash
cmake --build --preset plugin --target rompler_tests
./build-plugin/plugins/rompler/test/rompler_tests "[rt][bank]"
cmake --preset asan
cmake --build --preset asan --target rompler_tests
ASAN_OPTIONS=detect_leaks=0 ./build-asan/plugins/rompler/test/rompler_tests "[rt][bank]"
```

Expected: focused tests pass, replacement remains bounded, and ASan/UBSan reports no use-after-free or invalid access. If the preset names a different sanitizer build directory, use the directory printed by `cmake --preset asan` and record it in the commit message body.

- [ ] **Step 5: Commit Task 5 files**

```bash
git add plugins/rompler/Source/SF2Loader.h plugins/rompler/Source/PluginProcessor.h plugins/rompler/Source/PluginProcessor.cpp plugins/rompler/Source/Sampler.h plugins/rompler/Source/Sampler.cpp plugins/rompler/test/test_rt_correctness.cpp
git diff --cached --check
git commit -m "fix(rompler): bound SoundFont bank lifetime"
```

### Task 6: Run Delivery 1 regression and release gates

**Files:**
- Modify only if evidence requires a focused regression: `plugins/rompler/test/test_rt_correctness.cpp`
- Create: `docs/superpowers/verification/2026-08-29-rt-correctness-delivery-1.md`

- [ ] **Step 1: Run all source-level tests**

```bash
cmake --preset plugin
cmake --build --preset plugin --target rompler_tests
./build-plugin/plugins/rompler/test/rompler_tests
ctest --test-dir build-plugin --output-on-failure
```

Record the exact test count, skipped fixture tests, failures, and exit codes. Do not treat skipped corpus tests as passes.

- [ ] **Step 2: Run sanitizer tests**

```bash
cmake --preset asan
cmake --build --preset asan --target rompler_tests
ASAN_OPTIONS=detect_leaks=0 ./build-asan/plugins/rompler/test/rompler_tests
```

Record the actual preset output directory and all sanitizer diagnostics.

- [ ] **Step 3: Build plugin formats and validate binaries**

```bash
cmake --build --preset plugin --target EONDS50_VST3 EONDS50_AU EONDS50_Standalone
ctest --test-dir build-plugin -R pluginval_vst3_strictness10 --output-on-failure
auval -v aumu EDS5 AoDv
```

If the component codes differ in the generated bundle, read them from `Info.plist`, rerun `auval` with those exact values, and record both commands. Build success, pluginval, and AU validation are separate gates.

- [ ] **Step 4: Perform bounded realtime and host checks**

In a DAW, record these checks separately: note-on/off offsets in a 512-sample block, CC71/74 automation, oversampling latency compensation, rapid Program Change `0..7`, 128-note stress, sustain/legato/retrigger, and repeated SoundFont replacement while notes release. A manual DAW check is not replaced by source tests.

- [ ] **Step 5: Write the verification ledger**

Create the verification document with headings `Source`, `Sanitizers`, `Builds`, `Signing and installation`, `pluginval`, `auval`, `DAW`, and `UI/listening`. Under each heading record command, revision, result, and any remaining limitation. Do not mark installation, signing, DAW, UI, or listening complete unless actually observed.

- [ ] **Step 6: Commit the evidence only after fresh verification**

```bash
git add docs/superpowers/verification/2026-08-29-rt-correctness-delivery-1.md
git diff --cached --check
git commit -m "docs: record Delivery 1 verification"
```

## Delivery 1 completion checklist

- [ ] MIDI Note On, Note Off, pitch wheel, controllers, and Program Change mutate state at clamped event offsets and preserve equal-offset order.
- [ ] UI notes enter at block position zero through the bounded SPSC queue; overflow drops the newest event and sets an observable flag.
- [ ] `processBlock()` contains no mutex, dynamic allocation, file I/O, preset resolution, `setValueNotifyingHost()`, or `setLatencySamples()`.
- [ ] CC71, CC74, quick slots, and latency use the coalesced message-thread handoff.
- [ ] Voice stealing follows inactive, quiet release, quiet active, stable age order and clears stale ownership before reuse.
- [ ] The prepared pool and public parameter both support `1..128` voices.
- [ ] Replaced banks stay alive for their voices and are reclaimed on the message thread after references reach zero.
- [ ] Sustain, legato, retrigger, all-notes-off, preset recall, finite output, full tests, sanitizer, plugin build, pluginval, AU, DAW, and UI/listening gates are reported independently.
