#include "x10/sf2/Sf2Flattener.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

#include "x10/sf2/Generators.h"

namespace x10::sf2
{
namespace
{

using GenArray = std::array<std::int32_t, genCount>;

struct Range
{
    int low  = 0;
    int high = 127;
};

/**
    Generators whose amount field is unsigned.

    Reading one of these as signed turns a large index or a loud attenuation into
    a negative number, which then either selects the wrong sample or silences the
    region. The split is small enough to enumerate and too important to infer.
*/
[[nodiscard]] constexpr bool isUnsignedGenerator (std::uint16_t oper) noexcept
{
    switch (oper)
    {
        case genInstrument:
        case genSampleID:
        case genKeyRange:
        case genVelRange:
        case genSampleModes:
        case genExclusiveClass:
        case genInitialAttenuation:
        case genScaleTuning:
        case genInitialFilterQ:
        case genChorusEffectsSend:
        case genReverbEffectsSend:
        case genSustainVolEnv:
        case genSustainModEnv:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] std::int32_t amountOf (const Generator& generator) noexcept
{
    return isUnsignedGenerator (generator.oper)
             ? static_cast<std::int32_t> (generator.amountU16)
             : static_cast<std::int32_t> (generator.amountS16);
}

// ---------------------------------------------------------------------------
// Unit conversions into the engine schema
// ---------------------------------------------------------------------------

[[nodiscard]] float timecentsToSeconds (std::int32_t timecents) noexcept
{
    const float clamped = std::clamp (static_cast<float> (timecents), -12000.0f, 8000.0f);
    return std::pow (2.0f, clamped / 1200.0f);
}

[[nodiscard]] float absoluteCentsToHz (std::int32_t cents) noexcept
{
    const float clamped = std::clamp (static_cast<float> (cents), 1500.0f, 13500.0f);
    return 8.176f * std::pow (2.0f, clamped / 1200.0f);
}

/** Volume sustain is stored as attenuation in centibels; the engine wants a level. */
[[nodiscard]] float sustainLevelFromAttenuation (std::int32_t centibels) noexcept
{
    const float clamped = std::clamp (static_cast<float> (centibels), 0.0f, 1440.0f);
    return std::pow (10.0f, -clamped / 200.0f);
}

/** Modulation sustain is stored as a percentage *decrease*, in 0.1% units. */
[[nodiscard]] float sustainLevelFromDecrease (std::int32_t tenthsOfPercent) noexcept
{
    return std::clamp (1.0f - static_cast<float> (tenthsOfPercent) / 1000.0f, 0.0f, 1.0f);
}

[[nodiscard]] float panFromTenthsOfPercent (std::int32_t value) noexcept
{
    return std::clamp (static_cast<float> (value) / 500.0f, -1.0f, 1.0f);
}

[[nodiscard]] instrument::LoopMode loopModeFrom (std::int32_t sampleModes) noexcept
{
    switch (sampleModes & 3)
    {
        case 1:  return instrument::LoopMode::continuous;
        case 3:  return instrument::LoopMode::sustainThenRelease;
        default: return instrument::LoopMode::none; // 0 and the reserved 2
    }
}

// ---------------------------------------------------------------------------
// Zone accumulation
// ---------------------------------------------------------------------------

struct ZoneState
{
    GenArray values      = defaultGenerators();
    Range    keyRange    {};
    Range    velocityRange {};
    bool     hasInstrument = false;
    bool     hasSample     = false;
};

/**
    Folds one zone's generators into a state.

    @p presetLevel switches to the offset model: forbidden generators are
    dropped and the remaining amounts are accumulated rather than assigned,
    because a preset zone modifies whatever the instrument already decided.
*/
void applyZone (std::span<const Generator> generators, ZoneState& state, bool presetLevel)
{
    for (const auto& generator : generators)
    {
        if (generator.oper >= genCount)
            continue;

        // These two are what make a zone concrete rather than global, so they
        // must be recognised before the preset-level filter runs. Filtering
        // first would make every preset zone look global, the flattener would
        // treat each one as defaults for a zone that never comes, and the bank
        // would produce no regions at all while parsing perfectly.
        if (generator.oper == genInstrument)
        {
            state.hasInstrument = true;
            state.values[genInstrument] = amountOf (generator);
            continue;
        }

        if (generator.oper == genSampleID)
        {
            state.hasSample = true;
            state.values[genSampleID] = amountOf (generator);
            continue;
        }

        if (presetLevel && isForbiddenAtPresetLevel (generator.oper))
            continue;

        if (generator.oper == genKeyRange)
        {
            state.keyRange = Range { generator.amountLo, generator.amountHi };
            continue;
        }

        if (generator.oper == genVelRange)
        {
            state.velocityRange = Range { generator.amountLo, generator.amountHi };
            continue;
        }

        if (presetLevel)
            state.values[generator.oper] += amountOf (generator);
        else
            state.values[generator.oper] = amountOf (generator);
    }
}

/** Preset offsets start at zero, not at the generator defaults. */
[[nodiscard]] ZoneState makePresetOffsetState()
{
    ZoneState state;
    state.values.fill (0);
    return state;
}

[[nodiscard]] Range intersect (Range a, Range b) noexcept
{
    return Range { std::max (a.low, b.low), std::min (a.high, b.high) };
}

/** Half-open [first, last) span of a table, clamped to what actually exists. */
template <typename T>
[[nodiscard]] std::span<const T> slice (const std::vector<T>& table, std::size_t first, std::size_t last)
{
    first = std::min (first, table.size());
    last  = std::clamp (last, first, table.size());
    return std::span<const T> { table.data() + first, last - first };
}

[[nodiscard]] std::uint8_t toByte (std::int32_t value) noexcept
{
    return static_cast<std::uint8_t> (std::clamp (value, 0, 127));
}

} // namespace

std::vector<FlatPreset> flatten (const RawBank& bank, FlattenDiagnostics& diagnostics)
{
    diagnostics = FlattenDiagnostics{};

    std::vector<FlatPreset> result;
    result.reserve (bank.presets.size());

    for (std::size_t presetIndex = 0; presetIndex < bank.presets.size(); ++presetIndex)
    {
        const auto& presetHeader = bank.presets[presetIndex];

        FlatPreset flat;
        flat.name    = presetHeader.name;
        flat.bank    = presetHeader.bank;
        flat.program = presetHeader.preset;

        // The terminal phdr record was stripped by the reader, so the last
        // preset's zones run to the end of the bag table.
        const std::size_t firstBag = presetHeader.bagIndex;
        const std::size_t lastBag  = (presetIndex + 1 < bank.presets.size())
                                       ? bank.presets[presetIndex + 1].bagIndex
                                       : bank.presetBags.size();

        if (firstBag > lastBag || firstBag > bank.presetBags.size())
        {
            ++diagnostics.badBagRange;
            result.push_back (std::move (flat));
            continue;
        }

        const auto presetZones = slice (bank.presetBags, firstBag, lastBag);

        auto presetZoneGenerators = [&] (std::size_t zoneIndex) -> std::span<const Generator>
        {
            const std::size_t absolute = firstBag + zoneIndex;
            const std::size_t begin    = bank.presetBags[absolute].generatorIndex;
            const std::size_t end      = (absolute + 1 < bank.presetBags.size())
                                           ? bank.presetBags[absolute + 1].generatorIndex
                                           : bank.presetGenerators.size();
            return slice (bank.presetGenerators, begin, end);
        };

        // A leading zone that never names an instrument is the preset's global
        // zone: its generators are defaults for the zones that follow.
        ZoneState presetGlobal = makePresetOffsetState();
        std::size_t firstRealZone = 0;

        if (! presetZones.empty())
        {
            ZoneState probe = makePresetOffsetState();
            applyZone (presetZoneGenerators (0), probe, true);

            if (! probe.hasInstrument)
            {
                presetGlobal  = probe;
                firstRealZone = 1;
            }
        }

        for (std::size_t zoneIndex = firstRealZone; zoneIndex < presetZones.size(); ++zoneIndex)
        {
            ZoneState presetZone = presetGlobal;
            applyZone (presetZoneGenerators (zoneIndex), presetZone, true);

            if (! presetZone.hasInstrument)
            {
                ++diagnostics.zonesWithoutInstrument;
                continue;
            }

            // The instrument index is an absolute selection, not an offset, so it
            // is read from the zone's own generators rather than from the
            // accumulated preset state.
            std::int32_t instrumentIndex = -1;
            for (const auto& generator : presetZoneGenerators (zoneIndex))
                if (generator.oper == genInstrument)
                    instrumentIndex = static_cast<std::int32_t> (generator.amountU16);

            if (instrumentIndex < 0
                || static_cast<std::size_t> (instrumentIndex) >= bank.instruments.size())
            {
                ++diagnostics.badInstrumentIndex;
                continue;
            }

            const auto& instrumentHeader = bank.instruments[static_cast<std::size_t> (instrumentIndex)];

            const std::size_t firstInstrumentBag = instrumentHeader.bagIndex;
            const std::size_t lastInstrumentBag =
                (static_cast<std::size_t> (instrumentIndex) + 1 < bank.instruments.size())
                  ? bank.instruments[static_cast<std::size_t> (instrumentIndex) + 1].bagIndex
                  : bank.instrumentBags.size();

            if (firstInstrumentBag > lastInstrumentBag
                || firstInstrumentBag > bank.instrumentBags.size())
            {
                ++diagnostics.badBagRange;
                continue;
            }

            const auto instrumentZones = slice (bank.instrumentBags, firstInstrumentBag, lastInstrumentBag);

            auto instrumentZoneGenerators = [&] (std::size_t zone) -> std::span<const Generator>
            {
                const std::size_t absolute = firstInstrumentBag + zone;
                const std::size_t begin    = bank.instrumentBags[absolute].generatorIndex;
                const std::size_t end      = (absolute + 1 < bank.instrumentBags.size())
                                               ? bank.instrumentBags[absolute + 1].generatorIndex
                                               : bank.instrumentGenerators.size();
                return slice (bank.instrumentGenerators, begin, end);
            };

            ZoneState instrumentGlobal;
            std::size_t firstRealInstrumentZone = 0;

            if (! instrumentZones.empty())
            {
                ZoneState probe;
                applyZone (instrumentZoneGenerators (0), probe, false);

                if (! probe.hasSample)
                {
                    instrumentGlobal        = probe;
                    firstRealInstrumentZone = 1;
                }
            }

            for (std::size_t instrumentZone = firstRealInstrumentZone;
                 instrumentZone < instrumentZones.size();
                 ++instrumentZone)
            {
                ZoneState resolved = instrumentGlobal;
                applyZone (instrumentZoneGenerators (instrumentZone), resolved, false);

                if (! resolved.hasSample)
                {
                    ++diagnostics.zonesWithoutSample;
                    continue;
                }

                const auto sampleIndex = resolved.values[genSampleID];
                if (sampleIndex < 0
                    || static_cast<std::size_t> (sampleIndex) >= bank.sampleHeaders.size())
                {
                    ++diagnostics.badSampleIndex;
                    continue;
                }

                const auto& sampleHeader = bank.sampleHeaders[static_cast<std::size_t> (sampleIndex)];

                if (sampleHeader.isRom())
                {
                    // ROM samples live in hardware this project does not have.
                    ++diagnostics.romSamplesSkipped;
                    continue;
                }

                const Range keys      = intersect (resolved.keyRange, presetZone.keyRange);
                const Range velocities = intersect (resolved.velocityRange, presetZone.velocityRange);

                if (keys.low > keys.high || velocities.low > velocities.high)
                {
                    ++diagnostics.emptyKeyRange;
                    continue;
                }

                // Preset generators are offsets on top of the instrument's values.
                GenArray combined = resolved.values;
                for (std::size_t oper = 0; oper < genCount; ++oper)
                    if (! isForbiddenAtPresetLevel (static_cast<std::uint16_t> (oper)))
                        combined[oper] += presetZone.values[oper];

                instrument::Region region;

                region.keyLow       = toByte (keys.low);
                region.keyHigh      = toByte (keys.high);
                region.velocityLow  = toByte (velocities.low);
                region.velocityHigh = toByte (velocities.high);

                region.sampleIndex  = static_cast<std::uint32_t> (sampleIndex);
                region.sampleRateHz = static_cast<float> (sampleHeader.sampleRate);

                const auto offsetSum = [] (std::int32_t fine, std::int32_t coarse)
                {
                    return static_cast<std::int64_t> (fine)
                         + static_cast<std::int64_t> (coarse) * 32768;
                };

                const auto applyOffset = [] (std::uint32_t base, std::int64_t offset) -> std::uint32_t
                {
                    const std::int64_t shifted = static_cast<std::int64_t> (base) + offset;
                    return shifted < 0 ? 0u : static_cast<std::uint32_t> (shifted);
                };

                region.start = applyOffset (sampleHeader.start,
                                            offsetSum (combined[genStartAddrsOffset],
                                                       combined[genStartAddrsCoarseOffset]));
                region.end = applyOffset (sampleHeader.end,
                                          offsetSum (combined[genEndAddrsOffset],
                                                     combined[genEndAddrsCoarseOffset]));
                region.loopStart = applyOffset (sampleHeader.startLoop,
                                                offsetSum (combined[genStartloopAddrsOffset],
                                                           combined[genStartloopAddrsCoarseOffset]));
                region.loopEnd = applyOffset (sampleHeader.endLoop,
                                              offsetSum (combined[genEndloopAddrsOffset],
                                                         combined[genEndloopAddrsCoarseOffset]));

                region.loopMode = loopModeFrom (combined[genSampleModes]);

                // overridingRootKey is instrument-level only, so it is read from
                // the resolved instrument state rather than from the sum.
                const auto rootOverride = resolved.values[genOverridingRootKey];
                region.rootKey = (rootOverride >= 0)
                                   ? static_cast<float> (rootOverride)
                                   : static_cast<float> (sampleHeader.originalPitch);

                region.tuneCents = static_cast<float> (combined[genCoarseTune]) * 100.0f
                                 + static_cast<float> (combined[genFineTune])
                                 + static_cast<float> (sampleHeader.pitchCorrection);

                region.scaleTuningCentsPerKey = static_cast<float> (combined[genScaleTuning]);

                region.attenuationDb = static_cast<float> (combined[genInitialAttenuation]) / 10.0f;
                region.pan           = panFromTenthsOfPercent (combined[genPan]);

                region.filterCutoffHz    = absoluteCentsToHz (combined[genInitialFilterFc]);
                region.filterResonanceDb = static_cast<float> (combined[genInitialFilterQ]) / 10.0f;

                region.volumeEnvelope.delaySeconds   = timecentsToSeconds (combined[genDelayVolEnv]);
                region.volumeEnvelope.attackSeconds  = timecentsToSeconds (combined[genAttackVolEnv]);
                region.volumeEnvelope.holdSeconds    = timecentsToSeconds (combined[genHoldVolEnv]);
                region.volumeEnvelope.decaySeconds   = timecentsToSeconds (combined[genDecayVolEnv]);
                region.volumeEnvelope.releaseSeconds = timecentsToSeconds (combined[genReleaseVolEnv]);
                region.volumeEnvelope.sustainLevel   = sustainLevelFromAttenuation (combined[genSustainVolEnv]);

                region.modulationEnvelope.delaySeconds   = timecentsToSeconds (combined[genDelayModEnv]);
                region.modulationEnvelope.attackSeconds  = timecentsToSeconds (combined[genAttackModEnv]);
                region.modulationEnvelope.holdSeconds    = timecentsToSeconds (combined[genHoldModEnv]);
                region.modulationEnvelope.decaySeconds   = timecentsToSeconds (combined[genDecayModEnv]);
                region.modulationEnvelope.releaseSeconds = timecentsToSeconds (combined[genReleaseModEnv]);
                region.modulationEnvelope.sustainLevel   = sustainLevelFromDecrease (combined[genSustainModEnv]);

                region.modEnvToPitchCents  = static_cast<float> (combined[genModEnvToPitch]);
                region.modEnvToFilterCents = static_cast<float> (combined[genModEnvToFilterFc]);

                region.exclusiveClass = static_cast<std::uint8_t> (
                    std::clamp (resolved.values[genExclusiveClass], 0, 255));

                region.keyOverride      = resolved.values[genKeynum];
                region.velocityOverride = resolved.values[genVelocity];

                flat.regions.push_back (region);
            }
        }

        result.push_back (std::move (flat));
    }

    return result;
}

std::vector<FlatPreset> flatten (const RawBank& bank)
{
    FlattenDiagnostics ignored;
    return flatten (bank, ignored);
}

} // namespace x10::sf2
