#include "PluginProcessor.h"

namespace aod
{

RomplerProcessor::RomplerProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_ (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Zero until oversampling exists. When bus.osFactor starts selecting a real
    // halfband chain this must be updated and the host notified, or every other
    // track in the session drifts out of phase and the cause is invisible.
    setLatencySamples (0);
}

void RomplerProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    juce::ignoreUnused (sampleRate, maximumExpectedSamplesPerBlock);
}

void RomplerProcessor::releaseResources()
{
}

bool RomplerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // A synth takes no audio input. Hosts probing at strictness 10 will offer
    // layouts that make no sense for an instrument, and accepting them is how a
    // plugin ends up reading an input bus that was never allocated.
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void RomplerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    // M0 contract: silence. No allocation, no locking, no logging on this path.
    buffer.clear();
}

juce::AudioProcessorEditor* RomplerProcessor::createEditor()
{
    // The generic editor is the honest M0 answer: it exposes every parameter for
    // automation testing without pretending a designed UI exists. M5 replaces it.
    return new juce::GenericAudioProcessorEditor (*this);
}

void RomplerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts_.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RomplerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts_.state.getType()))
            apvts_.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace aod

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new aod::RomplerProcessor();
}
