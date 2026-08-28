# EON SF2 HQ, Stability, and Optimization Design

**Date:** 2026-08-28
**Status:** Approved direction; written specification pending user review
**Scope:** Playback fidelity, real-time correctness, CPU and memory efficiency, safe preset/SoundFont transitions, and measurable release validation

## Goal

Make EON SF2 sound cleaner and more faithful to its source SoundFonts while eliminating timing, transition, and real-time safety defects. The work must preserve existing projects and color controls, introduce scalable quality modes, and improve performance without moving filesystem, allocation, locking, UI, or host-notification work into the audio callback.

The target character is **transparent by default**. Drive, fold, nonlinear filtering, compression color, and effects remain available, but the quality upgrade must not make every preset brighter, louder, or more compressed merely to appear improved.

## Delivery Decomposition

This program is intentionally split into four independently testable deliveries. Each delivery receives its own implementation plan and can ship without waiting for the later deliveries.

1. **Real-time correctness and bug repair** — sample-accurate MIDI, lock-free UI notes, safe host handoff, correct voice allocation, honest polyphony, and safe bank lifetime.
2. **Transparent playback quality** — multi-region and stereo SF2 playback, higher-quality interpolation, loop treatment, parameter smoothing, and protected output headroom.
3. **CPU and memory optimization** — bypass-aware processing, cheaper pitch modulation, shared sample storage, asynchronous loading, and bounded bank reclamation.
4. **Quality modes and workflow optimization** — ECO/HQ/Render policies, atomic preset transitions, efficient dirty tracking and catalog updates, and full release profiling.

The first implementation plan covers delivery 1 only. Later deliveries begin only after the preceding delivery passes its stated gates, preventing broad DSP changes from hiding timing or lifetime regressions.

## Existing Baseline

The current engine already provides useful foundations:

- a fixed-size voice pool with sustain, legato, retrigger, and release behavior;
- bus nonlinear processing with prepared 1x, 2x, 4x, and 8x oversamplers;
- DC blocking, selected finite-value guards, and `ScopedNoDenormals`;
- compressor, chorus, reverb, and ping-pong delay stages;
- atomic publication of the active SoundFont loader;
- APVTS-backed parameter state and a user preset library;
- automated DSP, processor, preset, and UI component tests.

The baseline does not yet establish sample-accurate MIDI rendering, lock-free UI note delivery, complete multi-region/stereo SF2 reproduction, bounded loader retirement, or high-quality sample-rate conversion. Historical build or pluginval results remain revision-specific and must be rerun after each delivery.

## Delivery 1: Real-Time Correctness and Bug Repair

### Sample-accurate MIDI rendering

`RomplerProcessor::processBlock()` divides each host buffer at MIDI event sample positions. It renders audio from the previous cursor to the next event position, applies every event at that position in original order, then renders the remaining tail. Event positions are clamped to `[0, numSamples]` so malformed input cannot read outside the buffer.

This guarantees that Note On, Note Off, controller, pitch-wheel, program-change, sustain, and all-notes-off messages take effect at their declared position instead of being applied before the whole block. A Note On and Note Off within one block must produce only the intended subrange of audio.

### Lock-free UI note handoff

The message-thread keyboard uses a bounded single-producer/single-consumer queue with fixed-capacity storage. The audio callback may pop events but may not acquire a mutex, copy or destroy a `std::queue`, allocate, or block. Queue overflow drops the newest UI event and sets an atomic overflow flag for a non-modal UI diagnostic; host MIDI is never affected.

UI note events have block-boundary timing in this delivery because the UI does not provide a host sample timestamp. They enter before host MIDI events at sample position zero, preserving deterministic behavior.

### Host and message-thread handoff

The audio callback never calls `setValueNotifyingHost()` or performs preset resolution. CC71 and CC74 update dedicated real-time control state immediately. A bounded asynchronous message-thread handoff mirrors the latest value to APVTS and the editor without replaying every intermediate controller value.

Oversampling latency changes use a message-thread request. The selected prepared processor can change at a block boundary, but the host latency notification occurs outside the callback. Until the host acknowledges a latency-changing mode, transitions use the established latency policy and do not allocate.

MIDI Program Change `0–7` records a pending quick-slot request only. Preset lookup, SoundFont loading, parameter notification, and UI synchronization remain on the message thread.

### Voice ownership and stealing

Each voice records a monotonic start sequence and its current envelope level. Selection order is:

1. inactive voice;
2. releasing voice with the lowest current level;
3. active voice with the lowest estimated audibility, using envelope level first and age as a stable tie-breaker.

When a voice is reused, the old note-to-voice mapping is cleared before the new mapping is published. A short fixed fade or two-state steal transition prevents discontinuities without allocating. Lead legato voices and physically held sustained notes receive protection when an equally quiet alternative exists.

The exposed polyphony range and allocated capacity must agree. Delivery 1 uses a prepared maximum of 128 voices because the existing public parameter exposes `1–128`; reducing the public range would break saved automation expectations. Changing the limit never truncates arrays or accesses voices outside the prepared pool.

### SoundFont lifetime

Published SoundFont banks are immutable. Active voices keep a lifetime token for the bank that owns their sample. Replaced banks are reclaimed after the audio thread and all voices have released their tokens. The design must not depend on retaining every replaced loader until plug-in destruction.

Loading or removing a bank cannot leave an active voice with a dangling sample pointer. Failure to load a replacement leaves the current bank and audible state unchanged.

### Delivery 1 acceptance criteria

- MIDI offsets are correct for multiple events and same-block Note On/Off cases.
- The audio callback performs no mutex acquisition, dynamic allocation, file I/O, preset lookup, or host parameter notification.
- CC71, CC74, quick-slot requests, and oversampling latency updates reach the message thread without blocking audio.
- Voice stealing follows the documented order and clears stale ownership.
- Values `1–128` produce the corresponding prepared polyphony limit.
- Repeated SoundFont replacement has bounded memory once old voices finish.
- Existing sustain, legato, retrigger, all-notes-off, preset recall, and finite-output tests continue to pass.

## Delivery 2: Transparent Playback Quality

### Multi-region and stereo reproduction

Region matching returns every valid overlapping region up to a documented prepared maximum rather than a one-element result. Each Note On may create the voices required for layered regions, stereo-linked samples, and velocity overlaps. The engine applies region attenuation, pan, tuning, root-key override, loop mode, filter values, and stereo-link metadata that the parser exposes.

Unsupported SF2 generators or modulators are inventoried explicitly. They are not silently described as supported. The implementation prioritizes generators that materially affect audible reproduction: attenuation, pan, coarse/fine tuning, scale tuning, filter, envelopes, exclusive class, and linked stereo samples.

### Resampling and interpolation

Source PCM remains at its original sample rate in the canonical sample store. Playback rate combines source-to-host sample-rate conversion with root key, scale tuning, region tuning, pitch bend, and vibrato.

- **ECO interpolation:** four-point Hermite.
- **HQ interpolation:** bounded windowed-sinc or equivalent band-limited interpolation with coefficients prepared or table-driven outside the callback.
- **Render interpolation:** a higher-tap band-limited kernel selected only by an explicit mode or safe offline-render signal.

Downward and upward transposition must not introduce obvious image tones or unnecessary high-frequency loss. The quality comparison uses fixed input samples and spectral measurements rather than loudness-only listening.

### Loop treatment

Interpolation at a loop boundary reads the correct wrapped neighbor. Optional adaptive crossfades cover the end and start of looped material without exceeding the loop length. Release transitions from the loop to the available sample tail through a short crossfade when source data permits; otherwise the envelope finishes without holding a non-zero final sample indefinitely.

Malformed loop points remain clamped and cannot produce out-of-bounds reads, zero-length wrap arithmetic, or non-finite output.

### Smoothing and output protection

Audible continuous parameters use bounded ramps: output trim/mix, CC7/10/11, pitch bend, modulation depth, drive, fold, voice filter offset, bus filter, compressor threshold/ratio/makeup/mix, chorus controls, reverb controls, and delay feedback/mix. Discrete topology changes use short crossfades rather than numerical interpolation.

Tempo-driven delay changes crossfade between old and new fractional read positions. No delay-time jump may emit a discontinuity spike.

The transparent master path maintains internal headroom, removes DC and subsonic drift, and provides an optional safety limiter. The limiter is disabled or transparent at normal levels and is not used to make presets uniformly louder.

### Delivery 2 acceptance criteria

- Reference layered and stereo SoundFonts render all expected regions and channels.
- HQ interpolation has measurably lower alias energy than the current linear path in agreed pitch-shift fixtures.
- Default-path level and tonal response remain neutral within defined tolerances when color controls and effects are bypassed.
- Loop seams, parameter automation, tempo changes, preset changes, and voice steals do not produce discontinuity spikes above the test threshold.
- Output remains finite for supported sample rates, block sizes, parameter extremes, and malformed-but-accepted fixtures.

## Delivery 3: CPU and Memory Optimization

### Hot-path reductions

Pitch bend is shared at block or ramp granularity. Per-voice vibrato uses a recursive oscillator or prepared lookup rather than `sin()` and `pow()` for every voice sample. Filter and envelope coefficients update only when their inputs change.

Raw APVTS parameter pointers are cached during processor construction. Active voices are tracked directly so rendering does not repeatedly scan the entire prepared capacity when few voices sound. SIMD is applied only after algorithmic profiling identifies a stable, measurable loop.

### Bypass-aware DSP

Chorus, reverb, delay, compressor color, bus nonlinear processing, and oversampling expose explicit active/tail states. Fully bypassed processors do no unnecessary sample work. Effects with tails continue only until their state falls below a defined silence threshold.

Oversampling bypass preserves reported latency. A dry compensation path or fixed mode latency prevents timing changes when nonlinear controls move between exact bypass and active states.

### Shared sample storage and loading

PCM storage is keyed by source sample identity so regions referencing the same sample share one immutable buffer. Region objects store playback metadata and references rather than duplicate converted vectors.

File reading, parsing, validation, sample conversion, interpolation-table preparation, and optional cache creation run outside the audio callback. Completed immutable banks publish atomically. Loading supports cancellation, progress reporting, maximum file/sample/region limits, and a recoverable error result.

Cache identity includes canonical path, size, modification time or content hash, target format version, and quality-relevant conversion settings. Cache failure falls back to normal loading without damaging the source file.

### Delivery 3 acceptance criteria

- No allocation or lock occurs in repeated steady-state audio callbacks.
- Bypassed processing shows a measurable CPU reduction without changing compensated latency.
- A documented 32/64/128-voice benchmark records median and worst-case callback time at 44.1, 48, 96, and 192 kHz.
- Shared sample storage reduces duplicated PCM memory for a multi-region fixture.
- Loading large or malformed banks respects limits, remains cancellable, and does not interrupt current audio.

## Delivery 4: Quality Modes and Workflow Optimization

### Quality policy

Quality mode is engine state, not part of an audible user preset.

- **ECO:** Hermite interpolation, conservative nonlinear oversampling, 32-voice default budget.
- **HQ:** band-limited interpolation, 4x nonlinear oversampling default, 64-voice default budget.
- **Render:** highest verified interpolation kernel, up to 8x nonlinear oversampling, 128-voice budget when the host permits it.

Users may override the polyphony and nonlinear oversampling controls within safe prepared limits. Mode changes are glitch-free, report correct latency, and never rebuild DSP objects in the callback.

### Atomic preset transitions

A preset application is one transaction: resolve and prepare SoundFont, validate bank/program, prepare a parameter snapshot, publish the engine state, notify host parameters on the message thread, synchronize controls, clear transient knob readouts, and establish the correct `init` state. Any preparation failure leaves the previous sound active.

The audible transition uses a short crossfade. Existing notes may finish on the old immutable bank while new notes use the new snapshot. Delay and reverb tail behavior is deterministic and documented.

### UI and library efficiency

Preset dirty state uses parameter and engine revision counters rather than rebuilding and comparing a complete preset document on every UI timer tick. Library scans publish immutable catalog snapshots and update only changed files. Search is debounced and remains responsive with at least 1,000 presets.

Overflow, missing-SF2, cache, loading, and validation failures appear as non-modal messages and never block the audio thread.

### Delivery 4 acceptance criteria

- ECO, HQ, and Render policies select the documented algorithms and budgets.
- Switching quality mode or preset is click-free and does not allocate in the callback.
- Preset application updates audible state, host state, knob values, dirty state, and `init` presentation consistently.
- A 1,000-preset catalog does not require full document capture on every UI timer tick and remains responsive during search and updates.

## Error Handling and Compatibility

- Existing parameter IDs and value ranges remain stable unless a separate migration design is approved.
- New engine-state properties use versioned state with explicit defaults matching existing sessions.
- Invalid MIDI positions and values are clamped; invalid audio or coefficient values fall back to silence or the last safe value.
- Failed SF2, preset, or cache preparation never partially publishes a new engine.
- Queue overflow and resource-limit failures are observable but non-fatal.
- Existing saved sessions default to the closest behavior of the current engine; quality enhancements do not silently change enabled color controls.

## Verification Strategy

Each delivery uses failing-first tests for its behavior and separates evidence into these gates:

1. focused unit and component tests;
2. full rompler test executable and CTest suite;
3. sanitizer and malformed-input runs where supported;
4. VST3, AU, and Standalone builds;
5. bundle installation and code-signature inspection;
6. pluginval and AU validation at the project's required strictness;
7. DAW tests for MIDI timing, automation, latency compensation, rapid preset/bank changes, and offline render;
8. actual UI render inspection and listening/spectral comparison.

Performance tests record machine, build type, sample rate, block size, polyphony, quality mode, median callback time, high-percentile callback time, peak callback time, and memory footprint. A result from an older revision does not satisfy a later delivery.

## Out of Scope

- changing the artistic default to a permanently bright, compressed, or limited master;
- MPE, arbitrary MIDI learn, or a modulation-matrix redesign;
- cloud preset sync, marketplace, or content licensing;
- replacing JUCE or the existing SF2 parser wholesale;
- unrelated editor redesign;
- claiming complete SoundFont 2.x modulation support without explicit implementation and fixtures.
