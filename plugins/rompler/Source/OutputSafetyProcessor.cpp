#include "OutputSafetyProcessor.h"

#include <algorithm>
#include <cmath>

namespace aod
{

void OutputSafetyProcessor::prepare (double sampleRate, int maximumBlockSize, int numChannels)
{
    static_cast<void> (sampleRate);
    static_cast<void> (maximumBlockSize);
    static_cast<void> (numChannels);
    prepared_ = true;
}

void OutputSafetyProcessor::reset() noexcept
{
}

void OutputSafetyProcessor::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! prepared_ || buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return;

    // Apply only the last fraction of a dB of soft protection. This is
    // intentionally gain-transparent below the knee: a safety stage must not
    // turn the existing output trim into an undocumented makeup gain control.
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            data[sample] = applySoftCeiling (data[sample]);
    }
}

float OutputSafetyProcessor::applySoftCeiling (float sample) noexcept
{
    constexpr float knee = 0.97f;
    constexpr float ceiling = 0.985f;
    constexpr float response = 0.004f;

    const float magnitude = std::abs (sample);
    if (! std::isfinite (magnitude) || magnitude <= knee)
        return std::isfinite (sample) ? sample : 0.0f;

    const float excess = magnitude - knee;
    const float shaped = knee + (ceiling - knee) * (1.0f - std::exp (-excess / response));
    return std::copysign (std::min (shaped, ceiling), sample);
}

} // namespace aod
