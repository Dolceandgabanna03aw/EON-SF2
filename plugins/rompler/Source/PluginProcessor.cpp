#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace eon
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

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (voicePool_ != nullptr && sf2Loader_ != nullptr && numChannels > 0)
    {
        float* outL = buffer.getWritePointer(0);

        for (const auto event : midiMessages)
        {
            const auto msg = event.getMessage();

            if (msg.isNoteOn())
            {
                const int note = msg.getNoteNumber();
                const int velocity = msg.getVelocity();
                Sample* sample = sf2Loader_->getSample(getMidiBank(), getMidiProgram(), note, velocity);

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
                currentProgram_.store(msg.getProgramChangeNumber(), std::memory_order_relaxed);
            }
        }

        voicePool_->render(outL, numSamples, static_cast<int>(sampleRate_));

        if (numChannels > 1)
        {
            float* outR = buffer.getWritePointer(1);
            juce::FloatVectorOperations::copy(outR, outL, numSamples);
        }

        const auto outTrimParam = apvts_.getRawParameterValue(ParamIDs::outTrim);
        const float outTrimDb = outTrimParam ? outTrimParam->load() : 0.0f;
        const float outTrimGain = std::pow(10.0f, outTrimDb / 20.0f);

        buffer.applyGain(outTrimGain);
    }

    // Published for the editor's meter. Read on the message thread, so it is
    // stored even on the silent path — a stale peak would leave the meter lit
    // after the last voice ended.
    float peak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
        peak = juce::jmax (peak, buffer.getMagnitude (channel, 0, numSamples));

    peakLevel_.store (peak, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* RomplerProcessor::createEditor()
{
    return new RomplerEditor (*this);
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

bool RomplerProcessor::loadSoundFont(const juce::File& file)
{
    // Parsed and resampled on the calling thread. A bank takes far too long to
    // build to do it while the audio thread is blocked.
    auto incoming = std::make_unique<SF2Loader>(static_cast<int>(sampleRate_));

    if (! incoming->loadFile(file))
        return false;

    const auto [bank, program] = incoming->firstPresetProgram();

    {
        // Voices hold raw Sample pointers into the loader's map, so they have to
        // be released before the outgoing bank is freed. swap rather than assign:
        // assignment would destroy the old bank here, with the audio thread
        // waiting on the lock for the whole deallocation.
        const juce::ScopedLock audioLock (getCallbackLock());

        if (voicePool_ != nullptr)
            voicePool_->stopAll();

        sf2Loader_.swap (incoming);
        currentBank_.store (bank, std::memory_order_relaxed);
        currentProgram_.store (program, std::memory_order_relaxed);
    }
    // `incoming` now holds the outgoing bank, freed here outside the lock.
    loadedFileName_ = file.getFileName();
    return true;
}

} // namespace eon

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new eon::RomplerProcessor();
}
