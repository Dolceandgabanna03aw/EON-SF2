#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <x10/instrument/RegionIndex.h>
#include <x10/dsp/nonlinear/Curves.h>
#include <x10/dsp/filter/TptSvf.h>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace aod
{

struct Sample
{
    std::vector<float> data;
    int sampleRate = 48000;
    int loopStart = 0;
    int loopEnd = 0;
    bool loopEnabled = false;
    float filterCutoffHz = 19912.13f;
    float filterResonanceDb = 0.0f;
    // Pitch mapping: the sample plays back untransposed when the played MIDI
    // note equals rootKey. tuneCents is a constant offset; scaleTuningCentsPerKey
    // is the per-key pitch step (100 = normal chromatic, 0 = pinned to rootKey).
    float rootKey = 60.0f;
    float tuneCents = 0.0f;
    float scaleTuningCentsPerKey = 100.0f;
};

class Voice
{
public:
    /** Linear fade-out time after a note-off, from whatever level the envelope was at. */
    static constexpr float releaseTime = 0.08f;

    void start(const Sample* sample, int midiNote, float velocity) noexcept;
    /** Starts a short release fade; the voice deactivates itself once it reaches zero. */
    void stop() noexcept;
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    /** True while fading out from a note-off, before the slot is retired. */
    [[nodiscard]] bool isReleasing() const noexcept { return active_ && releasing_; }
    /** The note this voice is currently sounding (or -1 once it has no note). */
    [[nodiscard]] int note() const noexcept { return midiNote_; }
    /** How far the envelope has run, in seconds since the last start(). */
    [[nodiscard]] double envPhase() const noexcept { return envPhase_; }
    /** Monotonic stamp handed out by the pool at start(); smaller = started earlier. */
    [[nodiscard]] std::uint64_t startOrder() const noexcept { return startOrder_; }
    void setStartOrder(std::uint64_t order) noexcept { startOrder_ = order; }

    /**
        Per-voice drive (saturation) stage.

        driveAmount is the Drive knob as 0..1 (0..100 %). velToDrive is the
        Velocity-to-Drive knob as -1..1 (-100..+100 %). The effective amount is

            d = clamp(driveAmount * (1 + velToDrive * (velocity - 1)), 0, 1)

        so velocity scales the knob rather than adding to it: at +100 % a hit at
        half velocity gets half the drive, at -100 % softer hits get more, and
        0 % Drive is always clean regardless of velocity.

        The signal x is pushed into the selected curve with a pre-gain of
        maxDriveDb * d (0..+24 dB), the shaped signal is scaled so a sample at
        driveReferenceLevel (0.7, about -3 dBFS: a full-scale sample at velocity
        127 on the envelope's sustain) comes out at the same level, and the
        result is crossfaded with the dry signal by d:

            y = x + d * (curve(g * x) * ref / curve(g * ref) - x)

        Hence d = 0 is an exact bypass (unity, bit-identical to no drive stage)
        and raising d adds harmonics and compresses: peaks settle at about the
        reference level while quieter material is lifted, so the result gets
        louder and denser instead of quieter. Measured on a 1 kHz sine with the
        Tanh curve at 100 %: a -9 dBFS peak (0.35) comes out +8.5 dB RMS, a 0.7
        peak +2.7 dB, a full-scale peak -0.3 dB, with peaks capped near 0.7.
        Very quiet material (release tails, noise) can be lifted by up to the
        full small-signal gain g * ref / curve(g * ref), about +21 dB at 100 %.
    */
    static constexpr float maxDriveDb = 24.0f;
    static constexpr float driveReferenceLevel = 0.7f;

    void render(float* output, int numSamples, int hostSampleRate, float driveAmount, float velToDrive,
                int curveId, int filterRouting, float filterOffsetCents) noexcept;

private:
    const Sample* sample_ = nullptr;
    double phase_ = 0.0;
    float velocity_ = 0.0f;
    bool active_ = false;
    int midiNote_ = -1;
    std::uint64_t startOrder_ = 0;
    // Envelope time in seconds. double, not float: a float accumulating
    // 1/sampleRate stops advancing once it reaches 512 s (the increment falls
    // below half an ulp), which would freeze the release of a long-held note
    // and leave it ringing forever.
    double envPhase_ = 0.0;
    bool releasing_ = false;
    float releaseLevel_ = 0.0f;
    double releasePhase_ = 0.0;

    // Loop state: while looping (not releasing), phase_ wraps back to
    // loopStart_ once it passes loopEnd_. Cleared by start() so a retriggered
    // voice always begins from the sample head.
    int loopStart_ = 0;
    int loopEnd_ = 0;
    bool loopEnabled_ = false;

    // Pitch ratio from the note, rootKey and tunings, computed once at
    // start(). render() multiplies it by sampleRate / hostSampleRate, so a
    // sample that was not resampled to the current host rate (e.g. the host
    // rate changed and the bank could not be reloaded) still plays in tune.
    double playRate_ = 1.0;

    x10::dsp::TptSvf filter_;
    bool filterNeedsPrepare_ = true;
    int filterSampleRate_ = 0;

    [[nodiscard]] float envelope() const noexcept;
};

class VoicePool
{
public:
    static constexpr int maxVoices = 32;

    explicit VoicePool(int numVoices = maxVoices) : voices_(static_cast<std::size_t>(numVoices)) {}

    /** Caps the number of concurrently playing voices. Call from the audio thread. */
    void setPolyphony(int numVoices) noexcept;

    void start(const Sample* sample, int midiNote, float velocity) noexcept;
    void stop(int midiNote) noexcept;
    void stopAll() noexcept;

    /** True if a voice (sustaining or releasing) is currently sounding midiNote. */
    [[nodiscard]] bool isNoteActive(int midiNote) const noexcept;

    /** Stamp of the most recent start(); every later start() gets a larger one. */
    [[nodiscard]] std::uint64_t lastStartOrder() const noexcept { return nextStartOrder_; }

    /**
        True while any voice started at or before `stamp` is still active,
        including voices above the current polyphony cap. The processor uses
        this to tell when no voice can still be reading a retired loader's
        samples.
    */
    [[nodiscard]] bool anyActiveStartedAtOrBefore(std::uint64_t stamp) const noexcept;

    /** driveAmount 0..1 and velToDrive -1..1; see Voice::render() for the mapping. */
    void render(float* output, int numSamples, int hostSampleRate, float driveAmount, float velToDrive,
                int curveId, int filterRouting, float filterOffsetCents) noexcept;

private:
    std::vector<Voice> voices_;
    std::array<int, 128> noteToVoice_ {};
    int polyphony_ = static_cast<int>(voices_.size());
    // Incremented on every start() (including retriggers) and stamped onto the
    // voice, so stealing can pick the voice that was started longest ago.
    // 64-bit: at 1000 notes per second it would take ~585 million years to wrap.
    std::uint64_t nextStartOrder_ = 0;

    [[nodiscard]] Voice* findFreeVoice() noexcept;
};

} // namespace aod
