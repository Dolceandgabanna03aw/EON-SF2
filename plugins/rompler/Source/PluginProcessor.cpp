#include "PluginProcessor.h"

namespace aod
{

RomplerProcessor::RomplerProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_ (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    setLatencySamples (0);
}

void RomplerProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    juce::ignoreUnused (maximumExpectedSamplesPerBlock);
    sampleRate_ = sampleRate;
    voicePool_ = std::make_unique<VoicePool>();
}

void RomplerProcessor::releaseResources()
{
    voicePool_.reset();
}

bool RomplerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void RomplerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    if (!voicePool_ || !sf2Loader_)
        return;

    float* outL = buffer.getWritePointer(0);
    const int numSamples = buffer.getNumSamples();

    for (const auto event : midiMessages)
    {
        const auto msg = event.getMessage();

        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const int velocity = msg.getVelocity();
            Sample* sample = sf2Loader_->getSample(currentBank_, currentProgram_, note, velocity);

            if (sample != nullptr)
                voicePool_->start(sample, note, static_cast<float>(velocity) / 127.0f);
        }
        else if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();
            voicePool_->stop(note);
        }
        else if (msg.isProgramChange())
        {
            currentProgram_ = msg.getProgramChangeNumber();
        }
    }

    voicePool_->render(outL, numSamples, static_cast<int>(sampleRate_));

    if (buffer.getNumChannels() > 1)
    {
        float* outR = buffer.getWritePointer(1);
        juce::FloatVectorOperations::copy(outR, outL, numSamples);
    }

    const auto outTrimParam = apvts_.getRawParameterValue(ParamIDs::outTrim);
    const float outTrimDb = outTrimParam ? outTrimParam->load() : 0.0f;
    const float outTrimGain = std::pow(10.0f, outTrimDb / 20.0f);

    buffer.applyGain(outTrimGain);
}

juce::AudioProcessorEditor* RomplerProcessor::createEditor()
{
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

void RomplerProcessor::loadSoundFont(const juce::File& file)
{
    sf2Loader_ = std::make_unique<SF2Loader>(static_cast<int>(sampleRate_));
    if (sf2Loader_->loadFile(file))
    {
        const auto [bank, program] = sf2Loader_->firstPresetProgram();
        currentBank_ = bank;
        currentProgram_ = program;
    }
}

} // namespace aod

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new aod::RomplerProcessor();
}
