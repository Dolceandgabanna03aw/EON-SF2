#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "SF2Loader.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 512;

juce::File testSf2File()
{
    return juce::File (X10_SF2_CROSSCHECK_TESTDATA "/Dr._Mario_64_Soundfont.sf2");
}
} // namespace

TEST_CASE ("SF2Loader loads a real bank and resolves a sample for note-on", "[sf2][m1]")
{
    if (! testSf2File().existsAsFile())
        SKIP ("test SF2 corpus not present on this machine");

    aod::SF2Loader loader (static_cast<int> (kSampleRate));
    REQUIRE (loader.loadFile (testSf2File()));

    aod::Sample* sample = loader.getSample (0, 33, 60, 100);
    REQUIRE (sample != nullptr);
    REQUIRE (! sample->data.empty());
}

TEST_CASE ("a note-on through the processor produces non-silent output", "[sf2][m1]")
{
    if (! testSf2File().existsAsFile())
        SKIP ("test SF2 corpus not present on this machine");

    aod::RomplerProcessor processor;
    processor.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
    processor.prepareToPlay (kSampleRate, kBlockSize);
    processor.loadSoundFont (testSf2File());

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), 0);

    processor.processBlock (buffer, midi);

    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = std::max (peak, buffer.getMagnitude (ch, 0, kBlockSize));

    REQUIRE (peak > 0.0f);
}
