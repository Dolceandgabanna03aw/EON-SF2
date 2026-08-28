#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

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

TEST_CASE ("selecting a preset resets knobs but keeps oversampling", "[plugin][smoke]")
{
    aod::RomplerProcessor processor;
    auto& state = processor.getValueTreeState();
    const std::array<const char*, 27> knobIds {
        aod::ParamIDs::voiceDrive, aod::ParamIDs::voiceVelToDrive,
        aod::ParamIDs::voiceFilterOffset, aod::ParamIDs::busTapeDrive,
        aod::ParamIDs::busFold, aod::ParamIDs::busFilterCutoff,
        aod::ParamIDs::busFilterResonance, aod::ParamIDs::outTrim,
        aod::ParamIDs::outMix, aod::ParamIDs::fxChorusRate,
        aod::ParamIDs::fxChorusDepth, aod::ParamIDs::fxChorusMix,
        aod::ParamIDs::fxReverbRoom, aod::ParamIDs::fxReverbDamp,
        aod::ParamIDs::fxReverbMix, aod::ParamIDs::fxDelayMix,
        aod::ParamIDs::fxDelayFeedback, aod::ParamIDs::envAttack,
        aod::ParamIDs::envDecay, aod::ParamIDs::envSustain,
        aod::ParamIDs::envRelease, aod::ParamIDs::compThreshold,
        aod::ParamIDs::compRatio, aod::ParamIDs::compAttack,
        aod::ParamIDs::compRelease, aod::ParamIDs::compMakeup,
        aod::ParamIDs::compMix
    };
    for (const auto* id : knobIds)
    {
        auto* parameter = state.getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (0.13f);
    }
    auto* oversampling = state.getParameter (aod::ParamIDs::busOsFactor);
    REQUIRE (oversampling != nullptr);
    oversampling->setValueNotifyingHost (0.0f);

    processor.selectPreset (12, 34);

    for (const auto* id : knobIds)
    {
        const auto* parameter = state.getParameter (id);
        REQUIRE (std::abs (parameter->getValue() - parameter->getDefaultValue()) < 1.0e-5f);
    }
    REQUIRE (std::abs (oversampling->getValue()) < 1.0e-5f);
    const auto [bank, program] = processor.getCurrentBankProgram();
    REQUIRE (bank == 12);
    REQUIRE (program == 34);
}

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

TEST_CASE ("all notes off controller releases active voices", "[plugin][midi]")
{
    if (! testSf2File().existsAsFile())
        SKIP ("test SF2 corpus not present on this machine");

    aod::RomplerProcessor processor;
    processor.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
    processor.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    aod::SF2Loader loader (static_cast<int> (kSampleRate));
    REQUIRE (loader.loadFile (testSf2File()));
    const auto [bank, program] = loader.firstPresetProgram();
    int soundingKey = -1;
    for (int key = 0; key < 128 && soundingKey < 0; ++key)
        if (loader.getSample (bank, program, key, 100) != nullptr)
            soundingKey = key;
    REQUIRE (soundingKey >= 0);
    processor.loadSoundFont (testSf2File());
    processor.selectPreset (bank, program);

    midi.addEvent (juce::MidiMessage::noteOn (1, soundingKey, static_cast<juce::uint8> (100)), 0);
    processor.processBlock (buffer, midi);
    const float soundingPeak = buffer.getMagnitude (0, buffer.getNumSamples());
    REQUIRE (soundingPeak > 0.0f);

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 123, 0), 0);
    processor.processBlock (buffer, midi);

    float peak = buffer.getMagnitude (0, buffer.getNumSamples());
    for (int i = 0; i < 64 && peak > 1.0e-6f; ++i)
    {
        midi.clear();
        processor.processBlock (buffer, midi);
        peak = buffer.getMagnitude (0, buffer.getNumSamples());
    }

    // The processor's post-voice FX may retain a short tail, but the active
    // voice must be released rather than continuing at its original level.
    REQUIRE (peak < soundingPeak * 0.1f);
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
                            aod::ParamIDs::busFilterCutoff,
                            aod::ParamIDs::busFilterResonance,
                            aod::ParamIDs::busOsFactor,
                            aod::ParamIDs::outTrim,
                            aod::ParamIDs::outMix,
                            aod::ParamIDs::fxChorusRate,
                            aod::ParamIDs::fxChorusDepth,
                            aod::ParamIDs::fxChorusMix,
                            aod::ParamIDs::fxReverbRoom,
                            aod::ParamIDs::fxReverbDamp,
                            aod::ParamIDs::fxReverbMix,
                            aod::ParamIDs::fxDelayMix,
                            aod::ParamIDs::fxDelayFeedback,
                            aod::ParamIDs::envAttack,
                            aod::ParamIDs::envDecay,
                            aod::ParamIDs::envSustain,
                            aod::ParamIDs::envRelease,
                            aod::ParamIDs::compThreshold,
                            aod::ParamIDs::compRatio,
                            aod::ParamIDs::compAttack,
                            aod::ParamIDs::compRelease,
                            aod::ParamIDs::compMakeup,
                            aod::ParamIDs::compMix })
    {
        CAPTURE (id);
        REQUIRE (state.getParameter (id) != nullptr);
    }
}

TEST_CASE ("a new program starts with a conservative output trim", "[plugin][smoke]")
{
    aod::RomplerProcessor processor;
    const auto* outputTrim = processor.getValueTreeState().getRawParameterValue (aod::ParamIDs::outTrim);
    const auto* compressorMix = processor.getValueTreeState().getRawParameterValue (aod::ParamIDs::compMix);

    REQUIRE (outputTrim != nullptr);
    REQUIRE (compressorMix != nullptr);
    REQUIRE (std::abs (outputTrim->load() + 3.0f) < 1.0e-5f);
    REQUIRE (std::abs (compressorMix->load()) < 1.0e-5f);
}

TEST_CASE ("state survives a save and restore round trip", "[plugin][smoke]")
{
    aod::RomplerProcessor processor;
    auto& state = processor.getValueTreeState();

    auto* drive = state.getParameter (aod::ParamIDs::voiceDrive);
    auto* curve = state.getParameter (aod::ParamIDs::voiceCurve);
    auto* compressorRatio = state.getParameter (aod::ParamIDs::compRatio);
    auto* compressorMix = state.getParameter (aod::ParamIDs::compMix);
    REQUIRE (drive != nullptr);
    REQUIRE (curve != nullptr);
    REQUIRE (compressorRatio != nullptr);
    REQUIRE (compressorMix != nullptr);

    drive->setValueNotifyingHost (0.73f);
    curve->setValueNotifyingHost (1.0f);
    compressorRatio->setValueNotifyingHost (0.55f);
    compressorMix->setValueNotifyingHost (0.67f);

    const float savedDrive = drive->getValue();
    const float savedCurve = curve->getValue();
    const float savedCompressorRatio = compressorRatio->getValue();
    const float savedCompressorMix = compressorMix->getValue();

    juce::MemoryBlock blob;
    processor.getStateInformation (blob);
    REQUIRE (blob.getSize() > 0);

    // Move both away from the saved values so a no-op restore cannot pass.
    drive->setValueNotifyingHost (0.0f);
    curve->setValueNotifyingHost (0.0f);
    compressorRatio->setValueNotifyingHost (0.0f);
    compressorMix->setValueNotifyingHost (0.0f);

    processor.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));

    REQUIRE (std::abs (drive->getValue() - savedDrive) < 1.0e-5f);
    REQUIRE (std::abs (curve->getValue() - savedCurve) < 1.0e-5f);
    REQUIRE (std::abs (compressorRatio->getValue() - savedCompressorRatio) < 1.0e-5f);
    REQUIRE (std::abs (compressorMix->getValue() - savedCompressorMix) < 1.0e-5f);
}
