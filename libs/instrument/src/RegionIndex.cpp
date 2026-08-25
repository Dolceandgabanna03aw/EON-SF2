#include "x10/instrument/RegionIndex.h"

#include <algorithm>

namespace x10::instrument
{
namespace
{

[[nodiscard]] constexpr std::uint32_t combine (std::uint16_t bank, std::uint16_t program) noexcept
{
    return (static_cast<std::uint32_t> (bank) << 16) | static_cast<std::uint32_t> (program);
}

} // namespace

RegionIndexDiagnostics& RegionIndex::unusedDiagnostics() noexcept
{
    // thread_local rather than a plain function-local static: construction can
    // legitimately happen from more than one loader thread, and a shared
    // static here would let two loads race on the same counters.
    thread_local RegionIndexDiagnostics ignored;
    ignored = RegionIndexDiagnostics{};
    return ignored;
}

RegionIndex::RegionIndex (std::vector<Preset> presets) noexcept
    : RegionIndex (std::move (presets), unusedDiagnostics())
{
}

RegionIndex::RegionIndex (std::vector<Preset> presets, RegionIndexDiagnostics& diagnostics) noexcept
    : presets_ (std::move (presets))
{
    diagnostics = RegionIndexDiagnostics{};

    keyBuckets_.resize (presets_.size());
    sortedKeys_.reserve (presets_.size());

    for (std::size_t presetIndex = 0; presetIndex < presets_.size(); ++presetIndex)
    {
        const auto& preset = presets_[presetIndex];
        auto& buckets = keyBuckets_[presetIndex];

        for (std::size_t regionIndex = 0; regionIndex < preset.regions.size(); ++regionIndex)
        {
            const auto& region = preset.regions[regionIndex];

            // Defensive rather than assumed: RegionIndex is meant to serve any
            // importer, and an SFZ or hand-built preset has no guarantee that
            // upstream validation (like the SF2 flattener's emptyKeyRange
            // check) already ran.
            if (region.keyLow > region.keyHigh)
                continue;

            const auto lo = std::clamp<int> (region.keyLow, 0, 127);
            const auto hi = std::clamp<int> (region.keyHigh, 0, 127);

            for (int key = lo; key <= hi; ++key)
                buckets[static_cast<std::size_t> (key)].push_back (static_cast<std::uint32_t> (regionIndex));
        }

        sortedKeys_.push_back (SortedKey { combine (preset.bank, preset.program), presetIndex });
    }

    // stable_sort, not sort: on a duplicate (bank, program) — which should not
    // happen in a well-formed bank, but nothing upstream guarantees it won't —
    // ties must keep their original relative order so "the first preset in the
    // input wins" is what findPresetIndex() actually returns, rather than
    // whichever the sort happened to place first.
    std::stable_sort (sortedKeys_.begin(), sortedKeys_.end(),
                      [] (const SortedKey& a, const SortedKey& b) { return a.combined < b.combined; });

    for (std::size_t i = 1; i < sortedKeys_.size(); ++i)
        if (sortedKeys_[i].combined == sortedKeys_[i - 1].combined)
            ++diagnostics.duplicatePresetKeys;
}

std::optional<std::size_t> RegionIndex::findPresetIndex (std::uint16_t bank, std::uint16_t program) const noexcept
{
    const auto key = combine (bank, program);

    const auto it = std::lower_bound (sortedKeys_.begin(), sortedKeys_.end(), key,
                                      [] (const SortedKey& entry, std::uint32_t k) { return entry.combined < k; });

    if (it == sortedKeys_.end() || it->combined != key)
        return std::nullopt;

    return it->presetIndex;
}

std::size_t RegionIndex::match (std::size_t presetIndex, int key, int velocity,
                                std::span<const Region*> out) const noexcept
{
    if (presetIndex >= presets_.size())
        return 0;

    if (key < 0 || key > 127)
        return 0;

    const auto& preset  = presets_[presetIndex];
    const auto& bucket  = keyBuckets_[presetIndex][static_cast<std::size_t> (key)];

    std::size_t matched = 0;
    std::size_t written = 0;

    for (const auto regionIndex : bucket)
    {
        const auto& region = preset.regions[regionIndex];

        // A region pinned by genKeynum/genVelocity matches only that exact
        // key/velocity, regardless of its keyLow/keyHigh range. Without this,
        // a pinned region (e.g. a drum sound fixed to one key) would also fire
        // on every other key inside its range, and a region whose pin sits
        // outside its nominal range would never fire at all.
        if (region.keyOverride >= 0 && region.keyOverride != key)
            continue;
        if (region.velocityOverride >= 0 && region.velocityOverride != velocity)
            continue;

        if (velocity < region.velocityLow || velocity > region.velocityHigh)
            continue;

        ++matched;

        if (written < out.size())
            out[written++] = &region;
    }

    return matched;
}

std::size_t RegionIndex::match (std::uint16_t bank, std::uint16_t program, int key, int velocity,
                                std::span<const Region*> out) const noexcept
{
    const auto presetIndex = findPresetIndex (bank, program);
    if (! presetIndex)
        return 0;

    return match (*presetIndex, key, velocity, out);
}

} // namespace x10::instrument
