#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace aod
{

/**
    Final zero-latency output ceiling for polyphonic and wet FX sums.

    This stage is deliberately after the audible FX and output trim. It keeps
    normal material below the ceiling essentially unchanged while catching
    short chord/reverb peaks that cannot be predicted from one voice alone.
*/
class OutputSafetyProcessor
{
public:
    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset() noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static float applySoftCeiling (float sample) noexcept;

    bool prepared_ = false;
};

} // namespace aod
