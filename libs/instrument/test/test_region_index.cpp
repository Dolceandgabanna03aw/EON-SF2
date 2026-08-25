#include "support/AllocationGuard.h" // must be first: installs the global operator new/delete override

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

#include "x10/instrument/RegionIndex.h"

using namespace x10::instrument;
using x10::instrument::test::AllocationScope;

namespace
{

[[nodiscard]] Region makeRegion (std::uint8_t keyLow, std::uint8_t keyHigh,
                                 std::uint8_t velLow = 0, std::uint8_t velHigh = 127) noexcept
{
    Region region;
    region.keyLow       = keyLow;
    region.keyHigh      = keyHigh;
    region.velocityLow  = velLow;
    region.velocityHigh = velHigh;
    return region;
}

[[nodiscard]] Preset makePreset (std::string name, std::uint16_t bank, std::uint16_t program,
                                 std::vector<Region> regions)
{
    Preset preset;
    preset.name    = std::move (name);
    preset.bank    = bank;
    preset.program = program;
    preset.regions = std::move (regions);
    return preset;
}

} // namespace

TEST_CASE ("a single full-range region matches every key and velocity", "[instrument][regionindex]")
{
    std::vector<Preset> presets;
    presets.push_back (makePreset ("Lead", 0, 0, { makeRegion (0, 127) }));

    RegionIndex index { std::move (presets) };
    const auto presetIndex = index.findPresetIndex (0, 0);
    REQUIRE (presetIndex.has_value());

    std::array<const Region*, 4> out {};

    REQUIRE (index.match (*presetIndex, 0, 0, out) == 1);
    REQUIRE (index.match (*presetIndex, 60, 100, out) == 1);
    REQUIRE (index.match (*presetIndex, 127, 127, out) == 1);
}

TEST_CASE ("preset lookup is exact and missing programs return nothing", "[instrument][regionindex]")
{
    std::vector<Preset> presets;
    presets.push_back (makePreset ("Piano", 0, 0, { makeRegion (0, 127) }));
    presets.push_back (makePreset ("Drums", 128, 0, { makeRegion (0, 127) }));
    presets.push_back (makePreset ("Bass",  0, 32, { makeRegion (0, 127) }));

    RegionIndex index { std::move (presets) };

    const auto piano = index.findPresetIndex (0, 0);
    const auto drums  = index.findPresetIndex (128, 0);
    const auto bass    = index.findPresetIndex (0, 32);

    REQUIRE (piano.has_value());
    REQUIRE (drums.has_value());
    REQUIRE (bass.has_value());

    REQUIRE (index.presetAt (*piano).name == "Piano");
    REQUIRE (index.presetAt (*drums).name == "Drums");
    REQUIRE (index.presetAt (*bass).name == "Bass");

    // Bank and program are both part of the key: (0, 0) and (128, 0) must not collide.
    REQUIRE (*piano != *drums);

    REQUIRE_FALSE (index.findPresetIndex (0, 1).has_value());
    REQUIRE_FALSE (index.findPresetIndex (1, 0).has_value());
}

TEST_CASE ("key ranges route to exactly the regions that cover them", "[instrument][regionindex]")
{
    std::vector<Preset> presets;
    presets.push_back (makePreset ("Split", 0, 0,
                                   { makeRegion (0, 59),    // low
                                     makeRegion (60, 72),   // mid
                                     makeRegion (73, 127) })); // high

    RegionIndex index { std::move (presets) };
    const auto presetIndex = *index.findPresetIndex (0, 0);

    std::array<const Region*, 4> out {};

    // Boundary keys on both sides of each split point.
    REQUIRE (index.match (presetIndex, 59, 64, out) == 1);
    REQUIRE (out[0] == &index.presetAt (presetIndex).regions[0]);

    REQUIRE (index.match (presetIndex, 60, 64, out) == 1);
    REQUIRE (out[0] == &index.presetAt (presetIndex).regions[1]);

    REQUIRE (index.match (presetIndex, 72, 64, out) == 1);
    REQUIRE (out[0] == &index.presetAt (presetIndex).regions[1]);

    REQUIRE (index.match (presetIndex, 73, 64, out) == 1);
    REQUIRE (out[0] == &index.presetAt (presetIndex).regions[2]);

    REQUIRE (index.match (presetIndex, 0, 64, out) == 1);
    REQUIRE (index.match (presetIndex, 127, 64, out) == 1);
}

TEST_CASE ("velocity layers at the same key are filtered independently of key routing",
           "[instrument][regionindex]")
{
    std::vector<Preset> presets;
    presets.push_back (makePreset ("Layered", 0, 0,
                                   { makeRegion (0, 127, 0, 63),
                                     makeRegion (0, 127, 64, 127) }));

    RegionIndex index { std::move (presets) };
    const auto presetIndex = *index.findPresetIndex (0, 0);

    std::array<const Region*, 4> out {};

    REQUIRE (index.match (presetIndex, 60, 0, out) == 1);
    REQUIRE (out[0] == &index.presetAt (presetIndex).regions[0]);

    REQUIRE (index.match (presetIndex, 60, 127, out) == 1);
    REQUIRE (out[0] == &index.presetAt (presetIndex).regions[1]);

    REQUIRE (index.match (presetIndex, 60, 63, out) == 1);
    REQUIRE (index.match (presetIndex, 60, 64, out) == 1);
}

TEST_CASE ("overlapping regions at the same key and velocity all match", "[instrument][regionindex]")
{
    // Deliberately stacked layers — a pad and a bright doubling on the same
    // note and velocity. RegionIndex must return both, not collapse to one.
    std::vector<Preset> presets;
    presets.push_back (makePreset ("Stack", 0, 0,
                                   { makeRegion (0, 127), makeRegion (0, 127), makeRegion (0, 127) }));

    RegionIndex index { std::move (presets) };
    const auto presetIndex = *index.findPresetIndex (0, 0);

    std::array<const Region*, 8> out {};
    REQUIRE (index.match (presetIndex, 60, 100, out) == 3);
}

TEST_CASE ("a buffer smaller than the match count reports how much was dropped",
           "[instrument][regionindex]")
{
    std::vector<Preset> presets;
    presets.push_back (makePreset ("Stack", 0, 0,
                                   { makeRegion (0, 127), makeRegion (0, 127), makeRegion (0, 127) }));

    RegionIndex index { std::move (presets) };
    const auto presetIndex = *index.findPresetIndex (0, 0);

    std::array<const Region*, 1> tiny {};
    const auto matched = index.match (presetIndex, 60, 100, tiny);

    REQUIRE (matched == 3);       // the true count, not the truncated one
    REQUIRE (tiny[0] != nullptr); // and the buffer was still filled as far as it goes
}

TEST_CASE ("a preset with no regions matches nothing without crashing", "[instrument][regionindex]")
{
    std::vector<Preset> presets;
    presets.push_back (makePreset ("Empty", 0, 0, {}));

    RegionIndex index { std::move (presets) };
    const auto presetIndex = *index.findPresetIndex (0, 0);

    std::array<const Region*, 4> out {};
    REQUIRE (index.match (presetIndex, 60, 100, out) == 0);
}

TEST_CASE ("out-of-range inputs are declined rather than read out of bounds", "[instrument][regionindex]")
{
    std::vector<Preset> presets;
    presets.push_back (makePreset ("Lead", 0, 0, { makeRegion (0, 127) }));

    RegionIndex index { std::move (presets) };
    const auto presetIndex = *index.findPresetIndex (0, 0);

    std::array<const Region*, 4> out {};

    SECTION ("key below zero")     { REQUIRE (index.match (presetIndex, -1, 64, out) == 0); }
    SECTION ("key above 127")      { REQUIRE (index.match (presetIndex, 200, 64, out) == 0); }
    SECTION ("preset index past the table")
    {
        REQUIRE (index.match (presetIndex + 1, 60, 64, out) == 0);
    }
}

TEST_CASE ("a region with an inverted key range is never indexed", "[instrument][regionindex]")
{
    // Defensive: RegionIndex serves any importer, not only the SF2 flattener
    // that already validates ranges before this point.
    std::vector<Preset> presets;
    presets.push_back (makePreset ("Broken", 0, 0, { makeRegion (80, 40) }));

    RegionIndex index { std::move (presets) };
    const auto presetIndex = *index.findPresetIndex (0, 0);

    std::array<const Region*, 4> out {};
    for (int key = 0; key <= 127; ++key)
        REQUIRE (index.match (presetIndex, key, 64, out) == 0);
}

TEST_CASE ("a duplicate (bank, program) keeps the first preset and is counted",
           "[instrument][regionindex]")
{
    std::vector<Preset> presets;
    presets.push_back (makePreset ("First",  0, 0, { makeRegion (0, 127) }));
    presets.push_back (makePreset ("Second", 0, 0, { makeRegion (0, 127) }));

    RegionIndexDiagnostics diagnostics;
    RegionIndex index { std::move (presets), diagnostics };

    REQUIRE (diagnostics.duplicatePresetKeys == 1);

    const auto presetIndex = index.findPresetIndex (0, 0);
    REQUIRE (presetIndex.has_value());
    REQUIRE (index.presetAt (*presetIndex).name == "First");
}

TEST_CASE ("the convenience overload matches the split findPresetIndex + match path",
           "[instrument][regionindex]")
{
    std::vector<Preset> presets;
    presets.push_back (makePreset ("Lead", 0, 0, { makeRegion (0, 127) }));

    RegionIndex index { std::move (presets) };

    std::array<const Region*, 4> out {};
    REQUIRE (index.match (0, 0, 60, 100, out) == 1);
    REQUIRE (index.match (1, 0, 60, 100, out) == 0); // no such bank
}

TEST_CASE ("match() performs no heap allocation", "[instrument][regionindex]")
{
    // The real property under test: RegionIndex is documented as safe to call
    // from the audio thread. This is what makes that a checked fact rather
    // than a comment someone could invalidate in a later refactor.
    std::vector<Preset> presets;
    presets.push_back (makePreset ("Split", 0, 0,
                                   { makeRegion (0, 59, 0, 63),
                                     makeRegion (0, 59, 64, 127),
                                     makeRegion (60, 127, 0, 63),
                                     makeRegion (60, 127, 64, 127) }));

    RegionIndex index { std::move (presets) };
    const auto presetIndex = *index.findPresetIndex (0, 0);

    std::array<const Region*, 8> out {};

    const AllocationScope scope;
    for (int i = 0; i < 1000; ++i)
    {
        [[maybe_unused]] const auto matched = index.match (presetIndex, (i * 7) % 128, (i * 13) % 128, out);
        [[maybe_unused]] const auto found   = index.findPresetIndex (0, 0);
    }

    REQUIRE (scope.allocationsSoFar() == 0);
}

TEST_CASE ("a region pinned by keyOverride matches only that key", "[instrument][regionindex]")
{
    // A drum region fixed to key 60 (genKeynum) with a full key range: without
    // the override check it would also fire on every other key in 0..127.
    Region drum = makeRegion (0, 127);
    drum.keyOverride = 60;

    std::vector<Preset> presets;
    presets.push_back (makePreset ("Drums", 0, 0, { drum }));

    RegionIndex index { std::move (presets) };
    const auto presetIndex = *index.findPresetIndex (0, 0);

    std::array<const Region*, 4> out {};

    REQUIRE (index.match (presetIndex, 60, 100, out) == 1);  // the pinned key
    REQUIRE (index.match (presetIndex, 61, 100, out) == 0);  // other keys: silent
    REQUIRE (index.match (presetIndex, 0,  100, out) == 0);
    REQUIRE (index.match (presetIndex, 127, 100, out) == 0);
}

TEST_CASE ("a region pinned by velocityOverride matches only that velocity", "[instrument][regionindex]")
{
    Region region = makeRegion (0, 127);
    region.velocityOverride = 100;

    std::vector<Preset> presets;
    presets.push_back (makePreset ("PinnedVel", 0, 0, { region }));

    RegionIndex index { std::move (presets) };
    const auto presetIndex = *index.findPresetIndex (0, 0);

    std::array<const Region*, 4> out {};

    REQUIRE (index.match (presetIndex, 60, 100, out) == 1); // the pinned velocity
    REQUIRE (index.match (presetIndex, 60, 99,  out) == 0); // others: silent
    REQUIRE (index.match (presetIndex, 60, 101, out) == 0);
}

TEST_CASE ("Region::matches honours keyOverride and velocityOverride", "[instrument][regionindex]")
{
    Region region = makeRegion (10, 20);
    region.keyOverride      = 15;
    region.velocityOverride = 64;

    REQUIRE (region.matches (15, 64));  // both pins hit
    REQUIRE (! region.matches (15, 65)); // velocity pin misses
    REQUIRE (! region.matches (14, 64)); // key pin misses
    REQUIRE (! region.matches (9, 64));  // key below nominal range
    REQUIRE (! region.matches (21, 64)); // key above nominal range

    // Defaults (-1) keep the plain range behaviour.
    Region plain = makeRegion (10, 20);
    REQUIRE (plain.matches (10, 0));
    REQUIRE (plain.matches (20, 127));
    REQUIRE (! plain.matches (9, 0));
}
