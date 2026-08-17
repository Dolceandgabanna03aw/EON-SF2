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
    sampleRate_ = sampleRate;
    voicePool_ = std::make_unique<VoicePool>();

    busProcessor_.prepare (sampleRate, maximumExpectedSamplesPerBlock, getTotalNumOutputChannels());
    fxProcessor_.prepare (sampleRate, maximumExpectedSamplesPerBlock, getTotalNumOutputChannels());

    // The oversampling factor is fixed for this prepareToPlay session; see
    // BusProcessor::process for why it cannot change mid-stream without a
    // message-thread call. setLatencySamples() itself is safe to call here —
    // this runs before the host starts pumping audio through processBlock.
    const auto osFactorParam = apvts_.getRawParameterValue (ParamIDs::busOsFactor);
    cachedOsFactorIndex_ = osFactorParam ? static_cast<int> (osFactorParam->load()) : 2;
    setLatencySamples (busProcessor_.getLatencySamples (cachedOsFactorIndex_));
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

    // Drain UI / computer-keyboard note events first (message thread).
    std::queue<std::tuple<int, bool, int>> uiNotes;
    {
        const std::lock_guard lock (noteQueueMutex_);
        uiNotes = std::move (noteQueue_);
        noteQueue_ = {};
    }
    while (!uiNotes.empty())
    {
        const auto [note, on, velocity] = uiNotes.front();
        uiNotes.pop();
        if (on)
        {
            const int bank = currentBank_.load (std::memory_order_relaxed);
            const int program = currentProgram_.load (std::memory_order_relaxed);
            if (Sample* sample = loader->getSample (bank, program, note, velocity))
                voicePool_->start (sample, note, static_cast<float>(velocity) / 127.0f);
        }
        else
        {
            voicePool_->stop (note);
        }
    }

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

    const auto driveParam = apvts_.getRawParameterValue(ParamIDs::voiceDrive);
    const auto curveParam = apvts_.getRawParameterValue(ParamIDs::voiceCurve);
    const auto filterRoutingParam = apvts_.getRawParameterValue(ParamIDs::voiceFilterRouting);
    const auto filterOffsetParam = apvts_.getRawParameterValue(ParamIDs::voiceFilterOffset);
    const float driveDb = driveParam ? driveParam->load() : 0.0f;
    const int curveId = curveParam ? static_cast<int>(curveParam->load()) : 0;
    const int filterRouting = filterRoutingParam ? static_cast<int>(filterRoutingParam->load()) : 0;
    const float filterOffsetCents = filterOffsetParam ? filterOffsetParam->load() : 0.0f;

    voicePool_->render(outL, numSamples, static_cast<int>(sampleRate_), driveDb, curveId,
                        filterRouting, filterOffsetCents);

    if (buffer.getNumChannels() > 1)
    {
        float* outR = buffer.getWritePointer(1);
        juce::FloatVectorOperations::copy(outR, outL, numSamples);
    }

    const auto tapeDriveParam = apvts_.getRawParameterValue(ParamIDs::busTapeDrive);
    const auto foldParam = apvts_.getRawParameterValue(ParamIDs::busFold);
    const float tapeDrivePercent = tapeDriveParam ? tapeDriveParam->load() : 0.0f;
    const float foldPercent = foldParam ? foldParam->load() : 0.0f;

    busProcessor_.process (buffer, tapeDrivePercent, foldPercent, cachedOsFactorIndex_);

    const auto chorusRateP  = apvts_.getRawParameterValue (ParamIDs::fxChorusRate);
    const auto chorusDepthP = apvts_.getRawParameterValue (ParamIDs::fxChorusDepth);
    const auto chorusMixP   = apvts_.getRawParameterValue (ParamIDs::fxChorusMix);
    const auto reverbRoomP  = apvts_.getRawParameterValue (ParamIDs::fxReverbRoom);
    const auto reverbDampP  = apvts_.getRawParameterValue (ParamIDs::fxReverbDamp);
    const auto reverbMixP   = apvts_.getRawParameterValue (ParamIDs::fxReverbMix);

    fxProcessor_.process (buffer,
                          chorusRateP  ? chorusRateP->load()  : 1.0f,
                          chorusDepthP ? chorusDepthP->load() / 100.0f : 0.3f,
                          chorusMixP   ? chorusMixP->load()   / 100.0f : 0.25f,
                          reverbRoomP  ? reverbRoomP->load()  / 100.0f : 0.4f,
                          reverbDampP  ? reverbDampP->load()  / 100.0f : 0.5f,
                          reverbMixP   ? reverbMixP->load()   / 100.0f : 0.2f);

    const auto outTrimParam = apvts_.getRawParameterValue(ParamIDs::outTrim);
    const float outTrimDb = outTrimParam ? outTrimParam->load() : 0.0f;
    const float outTrimGain = std::pow(10.0f, outTrimDb / 20.0f);

    buffer.applyGain(outTrimGain);

    lastPeak_.store (buffer.getMagnitude (0, numSamples), std::memory_order_relaxed);
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

void RomplerProcessor::postNote (int note, bool on, int velocity)
{
    const std::lock_guard lock (noteQueueMutex_);
    if (noteQueue_.size() > 256)
        noteQueue_.pop();
    noteQueue_.push ({ note, on, velocity });
}

} // namespace aod

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new aod::RomplerProcessor();
}
