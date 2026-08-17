#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <x10/instrument/RegionIndex.h>
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
};

class Voice
{
public:
    void start(const Sample* sample, float velocity) noexcept;
    void stop() noexcept;
    [[nodiscard]] bool isActive() const noexcept { return active_; }

    void render(float* output, int numSamples, int hostSampleRate) noexcept;

private:
    const Sample* sample_ = nullptr;
    double phase_ = 0.0;
    float velocity_ = 0.0f;
    bool active_ = false;
    float envPhase_ = 0.0f;

    [[nodiscard]] float envelope() const noexcept;
};

class VoicePool
{
public:
    static constexpr int maxVoices = 32;

    explicit VoicePool(int numVoices = maxVoices) : voices_(static_cast<std::size_t>(numVoices)) {}

    void start(const Sample* sample, int midiNote, float velocity) noexcept;
    void stop(int midiNote) noexcept;
    void stopAll() noexcept;

    void render(float* output, int numSamples, int hostSampleRate) noexcept;

private:
    std::vector<Voice> voices_;
    std::array<int, 128> noteToVoice_ {};

    [[nodiscard]] Voice* findFreeVoice() noexcept;
};

} // namespace aod
