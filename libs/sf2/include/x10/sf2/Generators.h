#pragma once

#include <array>
#include <cstdint>

namespace x10::sf2
{

/** SoundFont generator operators. Gaps are unused or reserved by the spec. */
enum Gen : std::uint16_t
{
    genStartAddrsOffset           = 0,
    genEndAddrsOffset             = 1,
    genStartloopAddrsOffset       = 2,
    genEndloopAddrsOffset         = 3,
    genStartAddrsCoarseOffset     = 4,
    genModLfoToPitch              = 5,
    genVibLfoToPitch              = 6,
    genModEnvToPitch              = 7,
    genInitialFilterFc            = 8,
    genInitialFilterQ             = 9,
    genModLfoToFilterFc           = 10,
    genModEnvToFilterFc           = 11,
    genEndAddrsCoarseOffset       = 12,
    genModLfoToVolume             = 13,
    genChorusEffectsSend          = 15,
    genReverbEffectsSend          = 16,
    genPan                        = 17,
    genDelayModLfo                = 21,
    genFreqModLfo                 = 22,
    genDelayVibLfo                = 23,
    genFreqVibLfo                 = 24,
    genDelayModEnv                = 25,
    genAttackModEnv               = 26,
    genHoldModEnv                 = 27,
    genDecayModEnv                = 28,
    genSustainModEnv              = 29,
    genReleaseModEnv              = 30,
    genKeynumToModEnvHold         = 31,
    genKeynumToModEnvDecay        = 32,
    genDelayVolEnv                = 33,
    genAttackVolEnv               = 34,
    genHoldVolEnv                 = 35,
    genDecayVolEnv                = 36,
    genSustainVolEnv              = 37,
    genReleaseVolEnv              = 38,
    genKeynumToVolEnvHold         = 39,
    genKeynumToVolEnvDecay        = 40,
    genInstrument                 = 41,
    genKeyRange                   = 43,
    genVelRange                   = 44,
    genStartloopAddrsCoarseOffset = 45,
    genKeynum                     = 46,
    genVelocity                   = 47,
    genInitialAttenuation         = 48,
    genEndloopAddrsCoarseOffset   = 50,
    genCoarseTune                 = 51,
    genFineTune                   = 52,
    genSampleID                   = 53,
    genSampleModes                = 54,
    genScaleTuning                = 56,
    genExclusiveClass             = 57,
    genOverridingRootKey          = 58,

    genCount                      = 60
};

/**
    Default generator amounts.

    Most are zero, but the non-zero ones matter more than they look: an envelope
    stage defaulting to 0 timecents instead of -12000 is a one-second attack
    where the bank intended one millisecond, which sounds like a broken sampler
    rather than like a wrong constant.
*/
[[nodiscard]] constexpr std::array<std::int32_t, genCount> defaultGenerators() noexcept
{
    std::array<std::int32_t, genCount> defaults {};

    defaults[genInitialFilterFc] = 13500; // absolute cents, effectively open

    defaults[genDelayModLfo]  = -12000;
    defaults[genDelayVibLfo]  = -12000;

    defaults[genDelayModEnv]   = -12000;
    defaults[genAttackModEnv]  = -12000;
    defaults[genHoldModEnv]    = -12000;
    defaults[genDecayModEnv]   = -12000;
    defaults[genReleaseModEnv] = -12000;

    defaults[genDelayVolEnv]   = -12000;
    defaults[genAttackVolEnv]  = -12000;
    defaults[genHoldVolEnv]    = -12000;
    defaults[genDecayVolEnv]   = -12000;
    defaults[genReleaseVolEnv] = -12000;

    defaults[genKeynum]            = -1;
    defaults[genVelocity]          = -1;
    defaults[genScaleTuning]       = 100;
    defaults[genOverridingRootKey] = -1;

    return defaults;
}

/**
    Generators that a preset zone may not supply.

    A preset zone's generators are offsets applied on top of the instrument's
    absolute values. That model only makes sense for continuous quantities, so
    the spec forbids the rest at preset level. Applying a forbidden one anyway —
    adding sample offsets together, say — produces regions that point outside
    their own sample, which then reads as a corrupt bank rather than as a
    flattening bug.
*/
[[nodiscard]] constexpr bool isForbiddenAtPresetLevel (std::uint16_t oper) noexcept
{
    switch (oper)
    {
        case genStartAddrsOffset:
        case genEndAddrsOffset:
        case genStartloopAddrsOffset:
        case genEndloopAddrsOffset:
        case genStartAddrsCoarseOffset:
        case genEndAddrsCoarseOffset:
        case genStartloopAddrsCoarseOffset:
        case genEndloopAddrsCoarseOffset:
        case genKeynum:
        case genVelocity:
        case genSampleModes:
        case genExclusiveClass:
        case genOverridingRootKey:
        case genSampleID:
        case genInstrument:
            return true;
        default:
            return false;
    }
}

} // namespace x10::sf2
