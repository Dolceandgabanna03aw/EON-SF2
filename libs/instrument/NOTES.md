# RegionIndex — design notes

## Why a 128-bucket table instead of an interval structure

Key ranges overlap (velocity layers, round robins, deliberately stacked
doublings), so a plain sorted-array binary search on `keyLow` can't prune —
regions with a small `keyLow` and a huge `keyHigh` sort early but keep
matching for the rest of the table. A real interval-stabbing structure
(interval tree, segment tree) handles that correctly, but MIDI keys are a
domain of exactly 128 values known in advance. Precomputing, per preset, which
region indices cover each of the 128 keys turns note-on lookup into a single
array index — simpler than an interval tree, and the table only needs
rebuilding on bank load, never at runtime.

Space cost: 128 empty `std::vector`s per preset costs ~3 KB before any region
is inserted (empty vectors don't allocate). Irrelevant next to the sample data
these presets point at.

## Why the API is split into findPresetIndex() + match()

A program change happens rarely; a note-on happens constantly. Fusing them
into one call would mean paying the O(log n) preset search on every note, for
no reason — the preset a voice belongs to does not change between its note-on
and note-off. The intended calling pattern is: resolve the index once on
program change, cache it, pass it into `match()` for every note that follows.
A convenience overload that does both exists for callers that have not cached
anything yet (tests, one-shot tools); it is explicitly documented as not the
hot path.

## Why indices, not pointers, into presets_

An earlier draft had `match()` return a handle holding `const Preset*`. That
pointer is stable as long as `RegionIndex` isn't copied — a `std::vector` move
keeps its heap block, so pointers into `presets_` survive a `RegionIndex`
move — but copy support would need a full pointer-fixup pass, and it's a
sharp edge for anyone touching this code without rereading this note. Working
in `std::size_t presetIndex` throughout sidesteps the whole question: an index
is valid regardless of how the owning `RegionIndex` gets stored, moved, or
copied (copy is deleted anyway, but the index approach would have survived it
for free).

## The allocation-guard test

`match()` and `findPresetIndex()` are documented as real-time safe, meaning
in particular that neither allocates. That's a claim a later refactor can
break silently — a `std::vector` copy, an implicit `std::string` — without
any functional test noticing, since the outputs would still be correct, just
no longer safe to call from the audio thread. `test/support/AllocationGuard`
overrides global `operator new`/`delete` to count calls, so
"match() performs no heap allocation" checks a fact instead of trusting a
comment. Clang rejects a replacement `operator new`/`delete` declared
`inline` (`-Winline-new-delete`), so the overrides live in one `.cpp` linked
only into the test binary, never into `x10_instrument` itself.

## FlatPreset / instrument::Preset

`x10::sf2::FlatPreset` is a type alias for `x10::instrument::Preset`, not a
separate struct. `RegionIndex` takes `std::vector<instrument::Preset>`
directly — it has never heard of SF2, and a future SFZ importer populates the
exact same type without this header changing (planning document, issue A).
