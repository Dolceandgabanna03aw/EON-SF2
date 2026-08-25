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
    /** Short fade-out time used by the attack stage when a key is released during it. */
    static constexpr float releaseTime = 0.08f;

    void start(const Sample* sample, int midiNote, float velocity) noexcept;
    /** Starts a short release fade; the voice deactivates itself once it reaches zero. */
    void stop() noexcept;
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    /** True while fading out from a note-off, before the slot is retired. */
    [[nodiscard]] bool isReleasing() const noexcept { return active_ && releasing_; }
    /** The note this voice is currently sounding (or -1 once it has no note). */
    [[nodiscard]] int note() const noexcept { return midiNote_; }
    /** How far the envelope has run; used to pick the oldest voice when stealing. */
    [[nodiscard]] float envPhase() const noexcept { return envPhase_; }

    void render(float* output, int numSamples, int hostSampleRate, float driveDb, float velToDriveDb,
                int curveId, int filterRouting, float filterOffsetCents) noexcept;

private:
    const Sample* sample_ = nullptr;
    double phase_ = 0.0;
    float velocity_ = 0.0f;
    bool active_ = false;
    int midiNote_ = -1;
    float envPhase_ = 0.0f;
    bool releasing_ = false;
    float releaseLevel_ = 0.0f;
    float releasePhase_ = 0.0f;

    // Loop state: while looping (not releasing), phase_ wraps back to
    // loopStart_ once it passes loopEnd_. Cleared by start() so a retriggered
    // voice always begins from the sample head.
    int loopStart_ = 0;
    int loopEnd_ = 0;
    bool loopEnabled_ = false;

    // Playback rate in source frames per output sample. 1.0 plays the sample at
    // its recorded pitch; a higher MIDI note advances faster, a lower one
    // slower. Computed once at start() from the note, rootKey and tunings.
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

    void render(float* output, int numSamples, int hostSampleRate, float driveDb, float velToDriveDb,
                int curveId, int filterRouting, float filterOffsetCents) noexcept;

private:
    std::vector<Voice> voices_;
    std::array<int, 128> noteToVoice_ {};
    int polyphony_ = static_cast<int>(voices_.size());

    [[nodiscard]] Voice* findFreeVoice() noexcept;
};

} // namespace aod
