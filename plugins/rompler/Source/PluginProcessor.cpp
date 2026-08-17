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
    sampleRate_ = sampleRate;

    voicePool_ = std::make_unique<VoicePool>();
    voicePool_->prepare (sampleRate);

    busStage_.prepare (sampleRate, maximumExpectedSamplesPerBlock, 2);

    dryBuffer_.setSize (2, maximumExpectedSamplesPerBlock, false, false, true);
    wetBuffer_.setSize (2, maximumExpectedSamplesPerBlock, false, false, true);

    const auto maximumLatency = busStage_.latencySamples (static_cast<int> (BusStage::factors.size()) - 1);
    dryDelay_.setMaximumDelayInSamples (juce::jmax (1, maximumLatency + 1));
    dryDelay_.prepare ({ sampleRate, static_cast<juce::uint32> (maximumExpectedSamplesPerBlock), 2 });

    activeOversamplingIndex_ = -1;
}

void RomplerProcessor::releaseResources()
{
    voicePool_.reset();
    busStage_.reset();
    dryDelay_.reset();
}

void RomplerProcessor::handleAsyncUpdate()
{
    setLatencySamples (busStage_.latencySamples (activeOversamplingIndex_));
}

float RomplerProcessor::rawParameter (const char* parameterID, float fallback) const
{
    if (const auto* value = apvts_.getRawParameterValue (parameterID))
        return value->load();

    return fallback;
}

VoiceSettings RomplerProcessor::readVoiceSettings() const
{
    VoiceSettings settings;

    settings.drive01 = rawParameter (ParamIDs::voiceDrive, 20.0f) * 0.01f;
    settings.velocityToDrive = rawParameter (ParamIDs::voiceVelToDrive, 50.0f) * 0.01f;
    settings.curve = static_cast<VoiceCurve> (juce::jlimit (0, 2,
        static_cast<int> (rawParameter (ParamIDs::voiceCurve, 0.0f))));
    settings.filterBeforeDrive = rawParameter (ParamIDs::voiceFilterRouting, 0.0f) < 0.5f;
    settings.filterOffsetCents = rawParameter (ParamIDs::voiceFilterOffset, 0.0f);

    return settings;
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

    if (voicePool_ != nullptr && numChannels > 0 && numSamples <= dryBuffer_.getNumSamples())
    {
        const int oversamplingIndex = juce::jlimit (0, static_cast<int> (BusStage::factors.size()) - 1,
            static_cast<int> (rawParameter (ParamIDs::busOsFactor, 2.0f)));

        if (oversamplingIndex != activeOversamplingIndex_)
        {
            activeOversamplingIndex_ = oversamplingIndex;

            // setLatencySamples notifies the host, which is not something to do
            // from here; the async update runs it on the message thread.
            triggerAsyncUpdate();
        }

        voicePool_->setPolyphony (static_cast<int> (rawParameter (ParamIDs::polyLimit, 32.0f)));

        dryBuffer_.clear();
        wetBuffer_.clear();

        if (sf2Loader_ != nullptr)
        {
            for (const auto event : midiMessages)
            {
                const auto msg = event.getMessage();

                if (msg.isNoteOn())
                {
                    const int note = msg.getNoteNumber();
                    const int velocity = msg.getVelocity();
                    Sample* sample = sf2Loader_->getSample(getMidiBank(), getMidiProgram(), note, velocity);

                    if (sample != nullptr)
                        voicePool_->noteOn(sample, note, static_cast<float>(velocity) / 127.0f);
                }
                else if (msg.isNoteOff())
                {
                    voicePool_->noteOff(msg.getNoteNumber());
                }
                else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                {
                    voicePool_->stopAll();
                }
                else if (msg.isProgramChange())
                {
                    currentProgram_.store(msg.getProgramChangeNumber(), std::memory_order_relaxed);
                }
            }

            voicePool_->render (dryBuffer_.getWritePointer (0), dryBuffer_.getWritePointer (1),
                                wetBuffer_.getWritePointer (0), wetBuffer_.getWritePointer (1),
                                numSamples, readVoiceSettings());
        }

        juce::AudioBuffer<float> wetView (wetBuffer_.getArrayOfWritePointers(), 2, numSamples);
        busStage_.process (wetView,
                           rawParameter (ParamIDs::busTapeDrive, 0.0f) * 0.01f,
                           rawParameter (ParamIDs::busFold, 0.0f) * 0.01f,
                           oversamplingIndex);

        const int latency = busStage_.latencySamples (oversamplingIndex);

        const float mix = juce::jlimit (0.0f, 1.0f, rawParameter (ParamIDs::outMix, 100.0f) * 0.01f);
        const float outTrimGain = juce::Decibels::decibelsToGain (rawParameter (ParamIDs::outTrim, 0.0f));

        const float* dryLeft  = dryBuffer_.getReadPointer (0);
        const float* dryRight = dryBuffer_.getReadPointer (1);
        const float* wetLeft  = wetBuffer_.getReadPointer (0);
        const float* wetRight = wetBuffer_.getReadPointer (1);

        const bool mono = numChannels < 2;
        float* outLeft  = buffer.getWritePointer (0);
        float* outRight = mono ? nullptr : buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            // Both delay channels are advanced every sample regardless of the
            // host's layout, so a mono bus cannot desynchronise them.
            dryDelay_.pushSample (0, dryLeft[i]);
            dryDelay_.pushSample (1, dryRight[i]);

            const float alignedLeft  = dryDelay_.popSample (0, static_cast<float> (latency), true);
            const float alignedRight = dryDelay_.popSample (1, static_cast<float> (latency), true);

            const float left  = (alignedLeft  * (1.0f - mix) + wetLeft[i]  * mix) * outTrimGain;
            const float right = (alignedRight * (1.0f - mix) + wetRight[i] * mix) * outTrimGain;

            if (mono)
            {
                outLeft[i] = 0.5f * (left + right);
            }
            else
            {
                outLeft[i] = left;
                outRight[i] = right;
            }
        }

        for (int channel = 2; channel < numChannels; ++channel)
            buffer.clear (channel, 0, numSamples);
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
