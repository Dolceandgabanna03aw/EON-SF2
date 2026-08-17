#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "PluginProcessor.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 512;
} // namespace

/**
    JUCE is initialised and shut down here rather than through a function-local
    static.

    A lazily constructed static initialiser is destroyed during exit(), in an
    order unspecified relative to JUCE's own statics. That made the binary abort
    with SIGABRT after every test had already passed, intermittently — roughly
    one run in three. Owning the initialiser in main() makes both ends
    deterministic and on the main thread.
*/
int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    return Catch::Session().run (argc, argv);
}

TEST_CASE ("the processor renders silence without touching the real-time rules", "[plugin][smoke]")
{
    aod::RomplerProcessor processor;
    processor.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
    processor.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    // Fill with something loud first: a processor that forgets to clear its
    // output hands the host whatever was in the buffer, which in a real session
    // is the previous plugin's audio.
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample (channel, sample, 0.5f);

    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    processor.processBlock (buffer, midi);

    int nonFinite = 0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            nonFinite += (std::isfinite (buffer.getSample (channel, sample)) ? 0 : 1);

    REQUIRE (nonFinite == 0);

    // Written as <= rather than == 0.0f on purpose: JUCE's recommended warning
    // set propagates -Wfloat-equal into this target, and the inequality is exact
    // for this case anyway — magnitude is non-negative, so <= 0.0f holds only
    // for a buffer that is entirely +/-0.
    const float magnitude = buffer.getMagnitude (0, buffer.getNumSamples());
    CAPTURE (magnitude);
    REQUIRE (magnitude <= 0.0f);

    processor.releaseResources();
}

TEST_CASE ("the processor declares an instrument bus layout", "[plugin][smoke]")
{
    aod::RomplerProcessor processor;

    REQUIRE (processor.acceptsMidi());
    REQUIRE_FALSE (processor.producesMidi());
    REQUIRE_FALSE (processor.isMidiEffect());

    // Latency must stay honest. Once bus.osFactor drives a real halfband chain
    // this expectation changes, and it should change deliberately.
    REQUIRE (processor.getLatencySamples() == 0);

    // The processor declares no input bus at all, so a candidate layout must
    // carry zero input buses. Adding a disabled one makes the bus counts
    // disagree and the layout is rejected before isBusesLayoutSupported is even
    // consulted — which reads as a logic failure but is really a malformed query.
    juce::AudioProcessor::BusesLayout stereoOut;
    stereoOut.outputBuses.add (juce::AudioChannelSet::stereo());
    REQUIRE (processor.checkBusesLayoutSupported (stereoOut));

    juce::AudioProcessor::BusesLayout monoOut;
    monoOut.outputBuses.add (juce::AudioChannelSet::mono());
    REQUIRE (processor.checkBusesLayoutSupported (monoOut));

    // Exercises isBusesLayoutSupported itself: the bus count is right, the
    // channel set is one we do not handle.
    juce::AudioProcessor::BusesLayout surroundOut;
    surroundOut.outputBuses.add (juce::AudioChannelSet::create5point1());
    REQUIRE_FALSE (processor.checkBusesLayoutSupported (surroundOut));

    // An instrument must never be handed an input bus.
    juce::AudioProcessor::BusesLayout withInput;
    withInput.inputBuses.add (juce::AudioChannelSet::stereo());
    withInput.outputBuses.add (juce::AudioChannelSet::stereo());
    REQUIRE_FALSE (processor.checkBusesLayoutSupported (withInput));
}

TEST_CASE ("every declared parameter is reachable", "[plugin][smoke]")
{
    aod::RomplerProcessor processor;
    auto& state = processor.getValueTreeState();

    for (const auto* id : { aod::ParamIDs::voiceDrive,
                            aod::ParamIDs::voiceCurve,
                            aod::ParamIDs::voiceVelToDrive,
                            aod::ParamIDs::voiceFilterRouting,
                            aod::ParamIDs::voiceFilterOffset,
                            aod::ParamIDs::polyLimit,
                            aod::ParamIDs::busTapeDrive,
                            aod::ParamIDs::busFold,
                            aod::ParamIDs::busOsFactor,
                            aod::ParamIDs::outTrim,
                            aod::ParamIDs::outMix,
                            aod::ParamIDs::fxChorusRate,
                            aod::ParamIDs::fxChorusDepth,
                            aod::ParamIDs::fxChorusMix,
                            aod::ParamIDs::fxReverbRoom,
                            aod::ParamIDs::fxReverbDamp,
                            aod::ParamIDs::fxReverbMix })
    {
        CAPTURE (id);
        REQUIRE (state.getParameter (id) != nullptr);
    }
}

TEST_CASE ("state survives a save and restore round trip", "[plugin][smoke]")
{
    aod::RomplerProcessor processor;
    auto& state = processor.getValueTreeState();

    auto* drive = state.getParameter (aod::ParamIDs::voiceDrive);
    auto* curve = state.getParameter (aod::ParamIDs::voiceCurve);
    REQUIRE (drive != nullptr);
    REQUIRE (curve != nullptr);

    drive->setValueNotifyingHost (0.73f);
    curve->setValueNotifyingHost (1.0f);

    const float savedDrive = drive->getValue();
    const float savedCurve = curve->getValue();

    juce::MemoryBlock blob;
    processor.getStateInformation (blob);
    REQUIRE (blob.getSize() > 0);

    // Move both away from the saved values so a no-op restore cannot pass.
    drive->setValueNotifyingHost (0.0f);
    curve->setValueNotifyingHost (0.0f);

    processor.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));

    REQUIRE (std::abs (drive->getValue() - savedDrive) < 1.0e-5f);
    REQUIRE (std::abs (curve->getValue() - savedCurve) < 1.0e-5f);
}
