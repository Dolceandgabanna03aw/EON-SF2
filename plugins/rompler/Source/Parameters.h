#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

namespace aod
{

/**
    Parameter identifiers and the APVTS layout.

    The set follows the specification table in the planning document. Declaring
    them at M0, before any of the DSP exists, is deliberate: pluginval at
    strictness 10 hammers parameter round-tripping, automation and state
    restoration, so getting the declarations wrong is much cheaper to discover
    now than after the voice engine is built on top of them.

    The declarations are consumed by the voice, bus, dynamics, FX and output
    stages. Parameter identifiers stay stable so saved plugin state remains
    compatible as the engine grows.
*/
namespace ParamIDs
{
inline constexpr auto voiceDrive         = "voice.drive";
inline constexpr auto voiceCurve         = "voice.curve";
inline constexpr auto voiceVelToDrive    = "voice.velToDrive";
inline constexpr auto voiceFilterRouting = "voice.filterRouting";
inline constexpr auto voiceFilterOffset  = "voice.filterOffset";
inline constexpr auto voiceLegato        = "voice.legato";
inline constexpr auto polyLimit          = "poly.limit";
inline constexpr auto busTapeDrive       = "bus.tapeDrive";
inline constexpr auto busFold            = "bus.fold";
inline constexpr auto busFilterCutoff    = "bus.filterCutoff";
inline constexpr auto busFilterResonance = "bus.filterResonance";
inline constexpr auto busOsFactor        = "bus.osFactor";
inline constexpr auto outTrim            = "out.trim";
inline constexpr auto outMix             = "out.mix";
inline constexpr auto fxChorusRate       = "fx.chorusRate";
inline constexpr auto fxChorusDepth      = "fx.chorusDepth";
inline constexpr auto fxChorusMix        = "fx.chorusMix";
inline constexpr auto fxReverbRoom       = "fx.reverbRoom";
inline constexpr auto fxReverbDamp       = "fx.reverbDamp";
inline constexpr auto fxReverbMix        = "fx.reverbMix";
inline constexpr auto fxDelayMix         = "fx.delayMix";
inline constexpr auto fxDelayFeedback    = "fx.delayFeedback";
inline constexpr auto envAttack          = "env.attack";
inline constexpr auto envDecay           = "env.decay";
inline constexpr auto envSustain         = "env.sustain";
inline constexpr auto envRelease         = "env.release";
inline constexpr auto compThreshold      = "comp.threshold";
inline constexpr auto compRatio          = "comp.ratio";
inline constexpr auto compAttack         = "comp.attack";
inline constexpr auto compRelease        = "comp.release";
inline constexpr auto compMakeup         = "comp.makeup";
inline constexpr auto compMix            = "comp.mix";
} // namespace ParamIDs

/** Choice orderings, kept here so the DSP and the UI cannot disagree on them. */
namespace Choices
{
inline const juce::StringArray curve       { "Tanh", "Tube", "Transformer" };
inline const juce::StringArray filterRouting { "Pre", "Post" };
inline const juce::StringArray legato      { "Off", "Legato" };
inline const juce::StringArray osFactor    { "1x", "2x", "4x", "8x" };
} // namespace Choices

/** Canonical parameter groups shared by preset capture, reset and application. */
namespace ParamSets
{
inline constexpr std::array rotary {
    ParamIDs::voiceDrive, ParamIDs::voiceVelToDrive, ParamIDs::voiceFilterOffset,
    ParamIDs::busTapeDrive, ParamIDs::busFold, ParamIDs::busFilterCutoff,
    ParamIDs::busFilterResonance, ParamIDs::outTrim, ParamIDs::outMix,
    ParamIDs::fxChorusRate, ParamIDs::fxChorusDepth, ParamIDs::fxChorusMix,
    ParamIDs::fxReverbRoom, ParamIDs::fxReverbDamp, ParamIDs::fxReverbMix,
    ParamIDs::fxDelayMix, ParamIDs::fxDelayFeedback, ParamIDs::envAttack,
    ParamIDs::envDecay, ParamIDs::envSustain, ParamIDs::envRelease,
    ParamIDs::compThreshold, ParamIDs::compRatio, ParamIDs::compAttack,
    ParamIDs::compRelease, ParamIDs::compMakeup, ParamIDs::compMix };
inline constexpr std::array presetChoices {
    ParamIDs::voiceCurve, ParamIDs::voiceFilterRouting, ParamIDs::voiceLegato };
inline constexpr std::array engineOnly { ParamIDs::busOsFactor, ParamIDs::polyLimit };
} // namespace ParamSets

[[nodiscard]] inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout layout;

    const auto percent = String ("%");
    const auto cents   = String (" cents");
    const auto decibel = String (" dB");
    const auto hertz   = String (" Hz");
    const auto milliseconds = String (" ms");
    const auto ratioSuffix = String (" : 1");

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::voiceDrive, 1 }, "Drive",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 20.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::voiceCurve, 1 }, "Curve", Choices::curve, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::voiceVelToDrive, 1 }, "Velocity to Drive",
        NormalisableRange<float> { -100.0f, 100.0f, 0.01f }, 50.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::voiceFilterRouting, 1 }, "Filter Routing",
        Choices::filterRouting, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::voiceFilterOffset, 1 }, "Filter Offset",
        NormalisableRange<float> { -4800.0f, 4800.0f, 1.0f }, 0.0f,
        AudioParameterFloatAttributes{}.withLabel (cents)));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::voiceLegato, 1 }, "Legato",
        Choices::legato, 0));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { ParamIDs::polyLimit, 1 }, "Polyphony", 1, 128, 32));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::busTapeDrive, 1 }, "Tape Drive",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 0.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::busFold, 1 }, "Fold",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 0.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    // The bus filter remains sonically neutral for old sessions at its default
    // position.  Moving it into range enables the nonlinear resonant LPF in
    // BusProcessor, where it runs at the selected oversampling rate.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::busFilterCutoff, 1 }, "Bus Filter Cutoff",
        NormalisableRange<float> { 20.0f, 20000.0f, 1.0f, 0.25f }, 20000.0f,
        AudioParameterFloatAttributes{}.withLabel (hertz)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::busFilterResonance, 1 }, "Bus Filter Resonance",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 0.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    // Changing this will change the halfband filter delay once oversampling
    // exists, so it must drive setLatencySamples() and a host notification.
    // Latency is reported as zero for now because no oversampling is present.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::busOsFactor, 1 }, "Oversampling",
        Choices::osFactor, 2));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::outTrim, 1 }, "Output Trim",
        NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, -3.0f,
        AudioParameterFloatAttributes{}.withLabel (decibel)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::outMix, 1 }, "Mix",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 100.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::fxChorusRate, 1 }, "Chorus Rate",
        NormalisableRange<float> { 0.05f, 5.0f, 0.01f }, 1.0f,
        AudioParameterFloatAttributes{}.withLabel (hertz)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::fxChorusDepth, 1 }, "Chorus Depth",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 30.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::fxChorusMix, 1 }, "Chorus Mix",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 25.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::fxReverbRoom, 1 }, "Reverb Room",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 40.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::fxReverbDamp, 1 }, "Reverb Damp",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 50.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::fxReverbMix, 1 }, "Reverb Mix",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 20.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::fxDelayMix, 1 }, "Ping-Pong Delay Mix",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 0.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::fxDelayFeedback, 1 }, "Ping-Pong Delay Feedback",
        NormalisableRange<float> { 0.0f, 95.0f, 0.01f }, 35.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::envAttack, 1 }, "Envelope Attack",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 0.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::envDecay, 1 }, "Envelope Decay",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 50.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::envSustain, 1 }, "Envelope Sustain",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 100.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::envRelease, 1 }, "Envelope Release",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 50.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    // The dynamics section is deliberately dry by default. Existing sessions
    // that predate these parameters therefore retain their previous sound,
    // while a new patch presents the hardware-style compressor controls ready
    // to dial in.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::compThreshold, 1 }, "Compressor Threshold",
        NormalisableRange<float> { -48.0f, 0.0f, 0.1f }, -18.0f,
        AudioParameterFloatAttributes{}.withLabel (decibel)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::compRatio, 1 }, "Compressor Ratio",
        NormalisableRange<float> { 1.0f, 12.0f, 0.01f }, 3.0f,
        AudioParameterFloatAttributes{}.withLabel (ratioSuffix)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::compAttack, 1 }, "Compressor Attack",
        NormalisableRange<float> { 0.1f, 100.0f, 0.1f }, 15.0f,
        AudioParameterFloatAttributes{}.withLabel (milliseconds)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::compRelease, 1 }, "Compressor Release",
        NormalisableRange<float> { 20.0f, 1000.0f, 1.0f }, 180.0f,
        AudioParameterFloatAttributes{}.withLabel (milliseconds)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::compMakeup, 1 }, "Compressor Makeup",
        NormalisableRange<float> { -12.0f, 18.0f, 0.1f }, 0.0f,
        AudioParameterFloatAttributes{}.withLabel (decibel)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::compMix, 1 }, "Compressor Mix",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 0.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    return layout;
}

} // namespace aod
