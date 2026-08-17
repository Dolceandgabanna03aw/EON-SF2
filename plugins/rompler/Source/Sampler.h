#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <x10/dsp/Concepts.h>
#include <x10/dsp/filter/DCBlocker.h>
#include <x10/dsp/filter/TptSvf.h>
#include <x10/dsp/nonlinear/Adaa1.h>
#include <x10/dsp/nonlinear/Curves.h>
#include <x10/instrument/Region.h>

#include <array>
#include <cstdint>
#include <tuple>
#include <vector>

namespace eon
{

/** Voice curve selection. The order matches Choices::curve in Parameters.h. */
enum class VoiceCurve
{
    tanh = 0,
    tube,
    transformer
};

/**
    One region's audio, resampled to the host rate at load time, together with
    the region parameters the voice needs to play it.

    The region's own values are copied in rather than referenced: the voice runs
    on the audio thread and the RegionIndex that owns the Region can be swapped
    out from under it by a bank load.
*/
struct Sample
{
    std::vector<float> data;
    int sampleRate = 48000;
    int loopStart = 0;
    int loopEnd = 0;
    x10::instrument::LoopMode loopMode = x10::instrument::LoopMode::none;

    float rootKey = 60.0f;
    float tuneCents = 0.0f;
    float scaleTuningCentsPerKey = 100.0f;
    float attenuationDb = 0.0f;
    float pan = 0.0f;
    float filterCutoffHz = 19912.13f;
    float filterResonanceDb = 0.0f;

    x10::instrument::Envelope volumeEnvelope {};
};

/** Voice parameters, read once per block from the APVTS. */
struct VoiceSettings
{
    float drive01 = 0.2f;            ///< 0..1, maps to 0..kMaxDriveDb into the curve.
    float velocityToDrive = 0.5f;    ///< -1..1, how far velocity shifts drive01.
    VoiceCurve curve = VoiceCurve::tanh;
    bool filterBeforeDrive = true;   ///< Pre routing; false puts the filter after.
    float filterOffsetCents = 0.0f;
};

/** Drive at the top of the range, in dB into the curve. */
inline constexpr float kMaxDriveDb = 36.0f;

/**
    SoundFont volume envelope: delay, attack, hold, decay, sustain, release.

    Follows the SF2 convention rather than a linear ramp throughout — attack is
    linear in amplitude, decay and release are linear in decibels. A linear
    release on the amplitude audibly cuts the tail short, which is the usual
    reason a sampler sounds abrupt next to the bank's own player.
*/
class AmpEnvelope
{
public:
    void prepare (double sampleRate) noexcept;
    void noteOn (const x10::instrument::Envelope& shape) noexcept;
    void noteOff() noexcept;

    [[nodiscard]] float nextValue() noexcept;
    [[nodiscard]] bool isFinished() const noexcept { return stage_ == Stage::idle; }
    [[nodiscard]] bool isReleasing() const noexcept { return stage_ == Stage::release; }

    void reset() noexcept;

private:
    enum class Stage : std::uint8_t { idle, delay, attack, hold, decay, sustain, release };

    /** Floor for the dB-domain segments; below this the voice is considered done. */
    static constexpr float kSilenceDb = -96.0f;

    [[nodiscard]] int samplesFor (float seconds) const noexcept;
    void advanceStage() noexcept;

    double sampleRate_ = 48000.0;
    Stage stage_ = Stage::idle;

    x10::instrument::Envelope shape_ {};

    int samplesRemaining_ = 0;
    float level_ = 0.0f;
    float levelIncrement_ = 0.0f;   ///< Amplitude per sample, attack only.
    float decibels_ = kSilenceDb;
    float decibelIncrement_ = 0.0f; ///< dB per sample, decay and release.
    float sustainDb_ = 0.0f;
};

/**
    One playing note: sample playback, amplitude envelope, filter and the
    per-voice drive stage.

    Renders into two destinations at once. The dry pair carries the sampler
    output alone and the wet pair carries it through the drive stage, so
    out.mix can blend between them without a second pass over the sample.
*/
class Voice
{
public:
    void prepare (double sampleRate) noexcept;

    void start (const Sample* sample, int midiNote, float velocity, std::uint32_t age) noexcept;
    void release() noexcept;
    void kill() noexcept;

    [[nodiscard]] bool isActive() const noexcept { return active_; }
    [[nodiscard]] bool isReleasing() const noexcept { return envelope_.isReleasing(); }
    [[nodiscard]] int note() const noexcept { return note_; }
    [[nodiscard]] std::uint32_t age() const noexcept { return age_; }

    void render (float* dryL, float* dryR, float* wetL, float* wetR,
                 int numSamples, const VoiceSettings& settings) noexcept;

    void reset() noexcept;

private:
    template <class C>
    void renderCurve (float* dryL, float* dryR, float* wetL, float* wetR,
                      int numSamples, const VoiceSettings& settings,
                      float preGain, float postGain) noexcept;

    /** Interpolated read at the current phase; advances and handles looping. */
    [[nodiscard]] float nextSample() noexcept;

    const Sample* sample_ = nullptr;
    double phase_ = 0.0;
    double pitchRatio_ = 1.0;
    float velocity_ = 0.0f;
    float gainLeft_ = 0.707f;
    float gainRight_ = 0.707f;
    float staticGain_ = 1.0f;
    int note_ = -1;
    std::uint32_t age_ = 0;
    bool active_ = false;

    AmpEnvelope envelope_;
    x10::dsp::TptSvf filter_;
    x10::dsp::DCBlocker dcBlocker_;

    std::tuple<x10::dsp::Adaa1<x10::dsp::curves::Tanh>,
               x10::dsp::Adaa1<x10::dsp::curves::Tube>,
               x10::dsp::Adaa1<x10::dsp::curves::Transformer>> shapers_;
};

/**
    Fixed pool of voices with oldest-first stealing.

    Sized for the largest polyphony the parameter allows so that changing the
    limit never allocates; the limit selects how many of them are eligible.
*/
class VoicePool
{
public:
    static constexpr int maxVoices = 128;

    void prepare (double sampleRate) noexcept;
    void setPolyphony (int numVoices) noexcept;

    void noteOn (const Sample* sample, int midiNote, float velocity) noexcept;
    void noteOff (int midiNote) noexcept;
    void stopAll() noexcept;

    void render (float* dryL, float* dryR, float* wetL, float* wetR,
                 int numSamples, const VoiceSettings& settings) noexcept;

    [[nodiscard]] int activeVoiceCount() const noexcept;

private:
    [[nodiscard]] Voice* allocateVoice() noexcept;

    std::array<Voice, maxVoices> voices_ {};
    int polyphony_ = 32;
    std::uint32_t nextAge_ = 0;
};

} // namespace eon
