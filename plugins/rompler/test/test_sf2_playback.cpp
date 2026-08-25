#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "SF2Loader.h"

#include <algorithm>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 512;

/**
    Any bank in the local corpus will do.

    This used to name one specific file. That made the tests unrunnable
    anywhere but the machine that happened to hold that download — they skipped
    silently everywhere else, including on a fresh checkout, so the whole SF2
    playback path went unexercised without anything saying so. What these tests
    actually need is a real third-party bank, not a particular one.
*/
juce::File testSf2File()
{
    const juce::File corpus { X10_SF2_CROSSCHECK_TESTDATA };

    if (! corpus.isDirectory())
        return {};

    auto banks = corpus.findChildFiles (juce::File::findFiles, false, "*.sf2");

    if (banks.isEmpty())
        return {};

    // Sorted so a corpus holding several banks still picks the same one every
    // run: a test that changes subject between runs is worse than no test.
    std::sort (banks.begin(), banks.end());
    return banks.getFirst();
}
} // namespace

TEST_CASE ("SF2Loader loads a real bank and resolves a sample for note-on", "[sf2][m1]")
{
    if (! testSf2File().existsAsFile())
        SKIP ("test SF2 corpus not present on this machine");

    eon::SF2Loader loader;
    REQUIRE (loader.loadFile (testSf2File()));

    // Whichever preset the bank leads with, rather than a program number that
    // only one particular bank was known to carry.
    const auto [bank, program] = loader.firstPresetProgram();

    eon::Sample* sample = loader.getSample (bank, program, 60, 100);
    REQUIRE (sample != nullptr);
    REQUIRE (sample->data != nullptr);
    REQUIRE (! sample->data->empty());
}

TEST_CASE ("a note-on through the processor produces non-silent output", "[sf2][m1]")
{
    if (! testSf2File().existsAsFile())
        SKIP ("test SF2 corpus not present on this machine");

    eon::RomplerProcessor processor;
    processor.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
    processor.prepareToPlay (kSampleRate, kBlockSize);
    processor.loadSoundFont (testSf2File());

    juce::AudioBuffer<float> buffer (2, kBlockSize);

    // AudioBuffer does not zero its allocation, and a host always hands a synth
    // a cleared buffer. Without this the peak assertion below can be satisfied
    // by whatever was in the heap, which would pass even for a silent synth.
    buffer.clear();

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), 0);

    processor.processBlock (buffer, midi);

    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = std::max (peak, buffer.getMagnitude (ch, 0, kBlockSize));

    REQUIRE (peak > 0.0f);
}
