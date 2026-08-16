#pragma once

#include <cstdint>

namespace x10::instrument
{

/**
    The engine's own region schema.

    This deliberately does not mirror SoundFont generators. Every value here is
    already in engine units — seconds, hertz, decibels, cents — so that adding an
    SFZ or a native importer later means writing a sibling converter, not
    touching the voice engine (planning document, issue A). If a field here ever
    needs to be interpreted differently depending on which format it came from,
    that is a defect in the schema, not a feature.

    Defaults are the SoundFont defaults expressed in engine units, because that
    is the format being imported first; they are not part of the contract.
*/

enum class LoopMode : std::uint8_t
{
    none = 0,        ///< Play through and stop.
    continuous,      ///< Loop for the whole note.
    sustainThenRelease ///< Loop while held, then play to the end.
};

/**
    Six-stage envelope, in seconds. Sustain is a linear 0..1 level, not the
    attenuation the SoundFont format stores, so the voice engine never has to
    know which convention a bank used.
*/
struct Envelope
{
    float delaySeconds   = 0.001f;
    float attackSeconds  = 0.001f;
    float holdSeconds    = 0.001f;
    float decaySeconds   = 0.001f;
    float sustainLevel   = 1.0f;
    float releaseSeconds = 0.001f;
};

struct Region
{
    // ---- selection -------------------------------------------------------
    std::uint8_t keyLow      = 0;
    std::uint8_t keyHigh     = 127;
    std::uint8_t velocityLow = 0;
    std::uint8_t velocityHigh = 127;

    // ---- source ----------------------------------------------------------
    std::uint32_t sampleIndex = 0;
    std::uint32_t start       = 0;
    std::uint32_t end         = 0;
    std::uint32_t loopStart   = 0;
    std::uint32_t loopEnd     = 0;
    LoopMode      loopMode    = LoopMode::none;
    float         sampleRateHz = 44100.0f;

    // ---- pitch -----------------------------------------------------------
    /// MIDI note at which the sample plays back untransposed.
    float rootKey = 60.0f;
    /// Coarse tune, fine tune and the sample's own correction, already summed.
    float tuneCents = 0.0f;
    /// Cents of pitch change per key. 100 is normal; 0 pins every key to rootKey.
    float scaleTuningCentsPerKey = 100.0f;

    // ---- amplitude -------------------------------------------------------
    float attenuationDb = 0.0f;
    /// -1 hard left, +1 hard right.
    float pan = 0.0f;

    // ---- filter ----------------------------------------------------------
    float filterCutoffHz    = 19912.13f; ///< SoundFont default of 13500 absolute cents.
    float filterResonanceDb = 0.0f;

    // ---- envelopes -------------------------------------------------------
    Envelope volumeEnvelope {};
    Envelope modulationEnvelope {};
    float modEnvToPitchCents  = 0.0f;
    float modEnvToFilterCents = 0.0f;

    // ---- voice management ------------------------------------------------
    /// Non-zero groups regions that cut each other off, as hi-hats do.
    std::uint8_t exclusiveClass = 0;

    /// -1 means "use the key that was played"; otherwise the region is fixed.
    int keyOverride      = -1;
    int velocityOverride = -1;

    [[nodiscard]] bool matches (int key, int velocity) const noexcept
    {
        return key >= keyLow && key <= keyHigh
            && velocity >= velocityLow && velocity <= velocityHigh;
    }
};

} // namespace x10::instrument
