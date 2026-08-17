#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace eon
{

/**
    Parameter identifiers and the APVTS layout.

    The set follows the specification table in the planning document. Declaring
    them at M0, before any of the DSP exists, is deliberate: pluginval at
    strictness 10 hammers parameter round-tripping, automation and state
    restoration, so getting the declarations wrong is much cheaper to discover
    now than after the voice engine is built on top of them.

    Nothing here is connected to audio yet. processBlock outputs silence.
*/
namespace ParamIDs
{
inline constexpr auto voiceDrive         = "voice.drive";
inline constexpr auto voiceCurve         = "voice.curve";
inline constexpr auto voiceVelToDrive    = "voice.velToDrive";
inline constexpr auto voiceFilterRouting = "voice.filterRouting";
inline constexpr auto voiceFilterOffset  = "voice.filterOffset";
inline constexpr auto polyLimit          = "poly.limit";
inline constexpr auto busTapeDrive       = "bus.tapeDrive";
inline constexpr auto busFold            = "bus.fold";
inline constexpr auto busOsFactor        = "bus.osFactor";
inline constexpr auto outTrim            = "out.trim";
inline constexpr auto outMix             = "out.mix";
} // namespace ParamIDs

/** Choice orderings, kept here so the DSP and the UI cannot disagree on them. */
namespace Choices
{
inline const juce::StringArray curve       { "Tanh", "Tube", "Transformer" };
inline const juce::StringArray filterRouting { "Pre", "Post" };
inline const juce::StringArray osFactor    { "1x", "2x", "4x", "8x" };
} // namespace Choices

[[nodiscard]] inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout layout;

    const auto percent = String ("%");
    const auto cents   = String (" cents");
    const auto decibel = String (" dB");

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

    // Changing this will change the halfband filter delay once oversampling
    // exists, so it must drive setLatencySamples() and a host notification.
    // Latency is reported as zero for now because no oversampling is present.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::busOsFactor, 1 }, "Oversampling",
        Choices::osFactor, 2));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::outTrim, 1 }, "Output Trim",
        NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f,
        AudioParameterFloatAttributes{}.withLabel (decibel)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::outMix, 1 }, "Mix",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 100.0f,
        AudioParameterFloatAttributes{}.withLabel (percent)));

    return layout;
}

} // namespace eon
