#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "OutputSafetyProcessor.h"
#include "support/AllocationGuard.h"

TEST_CASE ("output safety caps post-FX overload at a true sample ceiling", "[dsp][output]")
{
    aod::OutputSafetyProcessor processor;
    processor.prepare (48000.0, 512, 2);

    juce::AudioBuffer<float> buffer (2, 512);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample (channel, sample, channel == 0 ? 2.0f : -1.8f);

    processor.process (buffer);

    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            peak = juce::jmax (peak, std::abs (buffer.getSample (channel, sample)));

    CAPTURE (peak);
    REQUIRE (std::isfinite (peak));
    REQUIRE (peak < 0.99f);
}

TEST_CASE ("output safety processing allocates nothing after prepare", "[dsp][rt][output]")
{
    aod::OutputSafetyProcessor processor;
    processor.prepare (48000.0, 512, 2);
    juce::AudioBuffer<float> buffer (2, 512);
    processor.process (buffer);

    const x10::instrument::test::AllocationScope scope;
    for (int pass = 0; pass < 32; ++pass)
        processor.process (buffer);

    REQUIRE (scope.allocationsSoFar() == 0);
}

TEST_CASE ("output safety leaves normal material at unity", "[dsp][output]")
{
    aod::OutputSafetyProcessor processor;
    processor.prepare (48000.0, 512, 2);
    juce::AudioBuffer<float> buffer (2, 512);
    buffer.clear();
    buffer.addSample (0, 0, 0.5f);
    buffer.addSample (1, 0, -0.5f);

    processor.process (buffer);

    REQUIRE (std::abs (buffer.getSample (0, 0) - 0.5f) < 1.0e-6f);
    REQUIRE (std::abs (buffer.getSample (1, 0) + 0.5f) < 1.0e-6f);
}
