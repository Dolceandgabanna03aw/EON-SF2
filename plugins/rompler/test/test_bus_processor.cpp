#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "BusProcessor.h"

namespace
{
constexpr int kSampleRate = 48000;
constexpr int kBlockSize = 2048;

float harmonicMagnitude (const juce::AudioBuffer<float>& buffer, float frequency)
{
    float cosine = 0.0f;
    float sine = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float phase = 2.0f * juce::MathConstants<float>::pi * frequency
                          * static_cast<float> (sample) / static_cast<float> (kSampleRate);
        const float value = buffer.getSample (0, sample);
        cosine += value * std::cos (phase);
        sine += value * std::sin (phase);
    }
    return std::sqrt (cosine * cosine + sine * sine) / static_cast<float> (buffer.getNumSamples());
}
} // namespace

TEST_CASE ("bus filter remains stable and adds analogue-style harmonics when resonating", "[dsp][bus]")
{
    juce::AudioBuffer<float> clean (2, kBlockSize);
    for (int sample = 0; sample < kBlockSize; ++sample)
    {
        const float value = 0.55f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f
                                              * static_cast<float> (sample) / static_cast<float> (kSampleRate));
        clean.setSample (0, sample, value);
        clean.setSample (1, sample, value);
    }

    auto filtered = clean;
    aod::BusProcessor processor;
    processor.prepare (kSampleRate, kBlockSize, 2);
    processor.process (filtered, 0.0f, 0.0f, 1200.0f, 82.0f, 2);

    for (int channel = 0; channel < filtered.getNumChannels(); ++channel)
        for (int sample = 0; sample < filtered.getNumSamples(); ++sample)
            REQUIRE (std::isfinite (filtered.getSample (channel, sample)));

    // A resonant analogue-style core is intentionally nonlinear. With a sine
    // input its moving, biased saturation must generate a measurable second
    // harmonic, unlike the transparent 20 kHz / zero-resonance default.
    const float secondHarmonic = harmonicMagnitude (filtered, 880.0f);
    CAPTURE (secondHarmonic);
    REQUIRE (secondHarmonic > 1.0e-4f);
}
