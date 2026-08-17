#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    SF2Loader* loader = activeLoader_.load (std::memory_order_acquire);
    if (!voicePool_ || loader == nullptr)
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
            const int bank = currentBank_.load (std::memory_order_relaxed);
            const int program = currentProgram_.load (std::memory_order_relaxed);
            Sample* sample = loader->getSample(bank, program, note, velocity);

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
            currentProgram_.store (msg.getProgramChangeNumber(), std::memory_order_relaxed);
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

void RomplerProcessor::loadSoundFont(const juce::File& file)
{
    auto newLoader = std::make_unique<SF2Loader>(static_cast<int>(sampleRate_));
    if (!newLoader->loadFile(file))
        return;

    const auto [bank, program] = newLoader->firstPresetProgram();
    currentBank_.store (bank, std::memory_order_relaxed);
    currentProgram_.store (program, std::memory_order_relaxed);
    loadedFileName_ = file.getFileName();

    // Publish the new loader before retiring the old one: a note-on on the
    // audio thread that reads activeLoader_ right now must see either the
    // fully-built new loader or the still-valid old one, never a half state.
    activeLoader_.store (newLoader.get(), std::memory_order_release);

    if (sf2Loader_)
        retiredLoaders_.push_back (std::move (sf2Loader_));
    sf2Loader_ = std::move (newLoader);
}

int RomplerProcessor::getPresetCount() const noexcept
{
    return sf2Loader_ ? sf2Loader_->presetCount() : 0;
}

juce::String RomplerProcessor::getPresetName (int presetIndex) const noexcept
{
    return sf2Loader_ ? sf2Loader_->presetName (presetIndex) : juce::String {};
}

std::pair<int, int> RomplerProcessor::getPresetBankProgram (int presetIndex) const noexcept
{
    return sf2Loader_ ? sf2Loader_->presetBankProgram (presetIndex) : std::pair<int, int> { 0, 0 };
}

void RomplerProcessor::selectPreset (int bank, int program) noexcept
{
    currentBank_.store (bank, std::memory_order_relaxed);
    currentProgram_.store (program, std::memory_order_relaxed);
}

} // namespace aod

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new aod::RomplerProcessor();
}
