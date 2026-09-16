#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <vector>

#include "DynamicsProcessor.h"

namespace
{
constexpr int kSampleRate = 48000;
constexpr int kBlockSize = 512;

void fillConstant (juce::AudioBuffer<float>& buffer, float leftValue, float rightValue)
{
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        buffer.setSample (0, sample, leftValue);
        if (buffer.getNumChannels() > 1)
            buffer.setSample (1, sample, rightValue);
    }
}

float tailPeak (const juce::AudioBuffer<float>& buffer, int channel, int tailSamples)
{
    const int firstSample = juce::jmax (0, buffer.getNumSamples() - tailSamples);
    float peak = 0.0f;

    for (int sample = firstSample; sample < buffer.getNumSamples(); ++sample)
        peak = juce::jmax (peak, std::abs (buffer.getSample (channel, sample)));

    return peak;
}
} // namespace

TEST_CASE ("dynamics processor preserves the input at zero mix", "[dsp][dynamics]")
{
    aod::DynamicsProcessor processor;
    processor.prepare (kSampleRate, kBlockSize, 2);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    std::vector<float> originalLeft (static_cast<std::size_t> (kBlockSize));
    std::vector<float> originalRight (static_cast<std::size_t> (kBlockSize));

    for (int sample = 0; sample < kBlockSize; ++sample)
    {
        const float phase = static_cast<float> (sample) / static_cast<float> (kBlockSize);
        const float left = std::sin (phase * juce::MathConstants<float>::twoPi) * 0.9f;
        const float right = std::cos (phase * juce::MathConstants<float>::twoPi) * 0.4f;
        buffer.setSample (0, sample, left);
        buffer.setSample (1, sample, right);
        originalLeft[static_cast<std::size_t> (sample)] = left;
        originalRight[static_cast<std::size_t> (sample)] = right;
    }

    processor.process (buffer, -40.0f, 20.0f, 0.1f, 40.0f, 12.0f, 0.0f);

    for (int sample = 0; sample < kBlockSize; ++sample)
    {
        const float leftDifference = std::abs (buffer.getSample (0, sample)
                                               - originalLeft[static_cast<std::size_t> (sample)]);
        const float rightDifference = std::abs (buffer.getSample (1, sample)
                                                - originalRight[static_cast<std::size_t> (sample)]);
        REQUIRE (leftDifference < 1.0e-7f);
        REQUIRE (rightDifference < 1.0e-7f);
    }
}

TEST_CASE ("dynamics processor reduces a steady loud signal", "[dsp][dynamics]")
{
    aod::DynamicsProcessor processor;
    processor.prepare (kSampleRate, kBlockSize, 2);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    for (int block = 0; block < 4; ++block)
    {
        fillConstant (buffer, 1.0f, 1.0f);
        processor.process (buffer, -20.0f, 10.0f, 0.01f, 80.0f, 0.0f, 100.0f);
    }

    REQUIRE (tailPeak (buffer, 0, 128) < 0.25f);
    REQUIRE (processor.getLastGainReductionDb() > 10.0f);
}

TEST_CASE ("dynamics processor uses one linked gain for left and right", "[dsp][dynamics]")
{
    aod::DynamicsProcessor processor;
    processor.prepare (kSampleRate, kBlockSize, 2);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    for (int block = 0; block < 4; ++block)
    {
        fillConstant (buffer, 1.0f, 0.25f);
        processor.process (buffer, -24.0f, 8.0f, 0.01f, 100.0f, 0.0f, 100.0f);
    }

    const float leftGain = buffer.getSample (0, kBlockSize - 1);
    const float rightGain = buffer.getSample (1, kBlockSize - 1) / 0.25f;
    // The compressor detector remains linked, while the reduction-dependent
    // asymmetric stage can add a very small channel-level difference.
    REQUIRE (std::abs (leftGain - rightGain) < 1.0e-3f);
}

TEST_CASE ("dynamics processor adds even harmonics as reduction increases", "[dsp][dynamics]")
{
    constexpr int samples = 2048;
    aod::DynamicsProcessor processor;
    processor.prepare (kSampleRate, samples, 1);

    juce::AudioBuffer<float> buffer (1, samples);
    for (int sample = 0; sample < samples; ++sample)
    {
        const float phase = juce::MathConstants<float>::twoPi
            * static_cast<float> (sample) / static_cast<float> (samples);
        buffer.setSample (0, sample, 0.85f * std::sin (phase));
    }

    processor.process (buffer, -30.0f, 12.0f, 0.0f, 80.0f, 0.0f, 100.0f);

    float secondHarmonic = 0.0f;
    for (int sample = 0; sample < samples; ++sample)
    {
        const float phase = juce::MathConstants<float>::twoPi
            * static_cast<float> (sample) / static_cast<float> (samples);
        secondHarmonic += buffer.getSample (0, sample) * std::cos (2.0f * phase);
    }

    REQUIRE (std::abs (secondHarmonic) > 0.5f);
}

TEST_CASE ("dynamics processor keeps extreme parameter outputs finite", "[dsp][dynamics]")
{
    aod::DynamicsProcessor processor;
    processor.prepare (kSampleRate, kBlockSize, 2);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    fillConstant (buffer, 4.0f, -3.0f);
    processor.process (buffer, -120.0f, 100.0f, 0.0f, 0.0f, 60.0f, 100.0f);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            REQUIRE (std::isfinite (buffer.getSample (channel, sample)));

    fillConstant (buffer, 0.5f, -0.5f);
    processor.process (buffer,
                       std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::infinity());

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            REQUIRE (std::isfinite (buffer.getSample (channel, sample)));
}

TEST_CASE ("dynamics processor reset clears reduction state", "[dsp][dynamics]")
{
    aod::DynamicsProcessor processor;
    processor.prepare (kSampleRate, kBlockSize, 2);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    fillConstant (buffer, 1.0f, 1.0f);
    processor.process (buffer, -24.0f, 8.0f, 0.01f, 100.0f, 0.0f, 100.0f);
    REQUIRE (processor.getLastGainReductionDb() > 1.0f);

    processor.reset();
    REQUIRE (std::abs (processor.getLastGainReductionDb()) < 1.0e-7f);

    fillConstant (buffer, 0.25f, -0.25f);
    processor.process (buffer, 0.0f, 4.0f, 0.0f, 100.0f, 0.0f, 100.0f);
    REQUIRE (std::abs (buffer.getSample (0, 0) - 0.25f) < 1.0e-7f);
    REQUIRE (std::abs (buffer.getSample (1, 0) + 0.25f) < 1.0e-7f);
}
