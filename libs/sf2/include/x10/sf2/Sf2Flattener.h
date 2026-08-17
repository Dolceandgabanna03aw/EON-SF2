#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "x10/instrument/Preset.h"
#include "x10/sf2/Sf2Reader.h"

namespace x10::sf2
{

/**
    One playable preset, already resolved down to flat regions.

    This is exactly x10::instrument::Preset. The alias exists so flatten()'s
    return type reads as an SF2 concept at the call site, while the type itself
    stays owned by the format-neutral instrument library — RegionIndex accepts
    a plain std::vector<instrument::Preset> and has no idea SF2 exists
    (planning document, issue A).
*/
using FlatPreset = instrument::Preset;

/** Counts of things that were skipped, so a quiet bank can be explained. */
struct FlattenDiagnostics
{
    std::size_t zonesWithoutInstrument = 0; ///< preset zones missing the instrument generator
    std::size_t zonesWithoutSample     = 0; ///< instrument zones missing sampleID
    std::size_t badInstrumentIndex     = 0;
    std::size_t badSampleIndex         = 0;
    std::size_t romSamplesSkipped      = 0;
    std::size_t emptyKeyRange          = 0; ///< preset and instrument ranges did not overlap
    std::size_t badBagRange            = 0;

    [[nodiscard]] std::size_t total() const noexcept
    {
        return zonesWithoutInstrument + zonesWithoutSample + badInstrumentIndex
             + badSampleIndex + romSamplesSkipped + emptyKeyRange + badBagRange;
    }
};

/**
    Resolves the preset -> instrument -> sample hierarchy into flat regions.

    Everything happens at load time, on purpose: a note-on must not walk a
    hierarchy, both because of the jitter and because the walk touches memory
    scattered across the whole bank (ADR-02).

    The three rules this implements are the ones that are easy to get subtly
    wrong, and wrong here means a bank that plays but is mistuned:

      - A zone is *global* when it lacks the generator that would make it
        concrete — an instrument generator at preset level, a sampleID at
        instrument level. Global values are defaults that the local zone
        overrides; they are not merged in some other order.
      - Instrument generators are absolute. Preset generators are offsets added
        on top, and only for the generators where that is meaningful.
      - Key and velocity ranges intersect between the two levels. They do not
        add, and a preset range narrower than the instrument's must win.

    Malformed input is skipped and counted rather than thrown or asserted; the
    diagnostics say how much was dropped so silence has an explanation.
*/
[[nodiscard]] std::vector<FlatPreset> flatten (const RawBank& bank,
                                               FlattenDiagnostics& diagnostics);

/** Convenience overload for callers that do not want the counts. */
[[nodiscard]] std::vector<FlatPreset> flatten (const RawBank& bank);

} // namespace x10::sf2
