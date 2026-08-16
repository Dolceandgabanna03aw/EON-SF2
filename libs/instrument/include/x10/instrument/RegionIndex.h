#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "x10/instrument/Preset.h"

namespace x10::instrument
{

/** How many (bank, program) collisions were resolved by keeping the first. */
struct RegionIndexDiagnostics
{
    std::size_t duplicatePresetKeys = 0;
};

/**
    Resolves (bank, program, key, velocity) to matching regions without
    spending any of the note-on's real-time budget.

    Everything that could allocate, throw, or walk a hierarchy happens once,
    here, at load time (ADR-02). What is left for the audio thread is a binary
    search over a few dozen presets and an array index into a precomputed
    128-entry table — both cheap and bounded, neither touching the heap.

    The API is split into two calls on purpose, because that is how the events
    that drive it actually arrive: findPresetIndex() runs on a program change,
    which is rare, and match() runs on every note-on, which is not. A caller
    that caches the index from the last program change never pays the O(log n)
    search per note.

    Built from a plain std::vector<Preset> rather than from x10::sf2::RawBank,
    so an SFZ importer or a native format can populate the same index without
    this header ever knowing SF2 exists (planning document, issue A).
*/
class RegionIndex
{
public:
    explicit RegionIndex (std::vector<Preset> presets) noexcept;
    RegionIndex (std::vector<Preset> presets, RegionIndexDiagnostics& diagnostics) noexcept;

    RegionIndex (const RegionIndex&)            = delete;
    RegionIndex& operator= (const RegionIndex&) = delete;
    RegionIndex (RegionIndex&&)                 = default;
    RegionIndex& operator= (RegionIndex&&)      = default;

    [[nodiscard]] std::size_t presetCount() const noexcept { return presets_.size(); }
    [[nodiscard]] const Preset& presetAt (std::size_t presetIndex) const noexcept { return presets_[presetIndex]; }

    /**
        O(log n) over the number of presets. Call this on program change, not
        per note — match() is the hot path.
    */
    [[nodiscard]] std::optional<std::size_t> findPresetIndex (std::uint16_t bank,
                                                               std::uint16_t program) const noexcept;

    /**
        Writes the regions matching @p key and @p velocity into @p out.

        O(1) average: one array index by key, then a linear scan over whatever
        shares that key — typically a handful of velocity layers or round
        robins, never the whole preset.

        Returns the number of regions that matched, which may exceed
        out.size(); only the first out.size() are written. A caller that sees
        a return value larger than the buffer it passed knows some matches
        were dropped and can retry with a bigger one. A spec-conformant
        instrument will not hit this in practice, but a corrupt or adversarial
        bank can still produce enough overlapping zones to, and dropping
        matches silently would turn into missing notes with no diagnostic.

        noexcept, no allocation, no lock: safe to call from the audio thread.
        An out-of-range presetIndex or a key outside 0..127 returns 0 rather
        than reading out of bounds.
    */
    [[nodiscard]] std::size_t match (std::size_t presetIndex, int key, int velocity,
                                     std::span<const Region*> out) const noexcept;

    /** Convenience for callers that have not cached a preset index. Not the hot path. */
    [[nodiscard]] std::size_t match (std::uint16_t bank, std::uint16_t program,
                                     int key, int velocity,
                                     std::span<const Region*> out) const noexcept;

private:
    static RegionIndexDiagnostics& unusedDiagnostics() noexcept;

    std::vector<Preset> presets_;

    /// keyBuckets_[presetIndex][key] holds indices into presets_[presetIndex].regions.
    std::vector<std::array<std::vector<std::uint32_t>, 128>> keyBuckets_;

    struct SortedKey
    {
        std::uint32_t combined; // (bank << 16) | program
        std::size_t   presetIndex;
    };
    std::vector<SortedKey> sortedKeys_;
};

} // namespace x10::instrument
