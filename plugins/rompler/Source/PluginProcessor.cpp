#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_core/juce_core.h>

#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#endif

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

    // Surface the bundled SoundFont so the plugin starts usable without a
    // manual Load step when the packaged font is present.
    loadBundledSoundFont();

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
    // The audio thread is guaranteed stopped here, so it is safe to retire
    // the loaders: processBlock() can no longer read activeLoader_ between
    // this store and the vector push below. Leaving the pointer set would
    // hand processBlock() a dangling reference if the host restarts audio
    // without a fresh prepareToPlay().
    //
    // The loader stays alive in retiredLoaders_ (never deleted here): voices
    // may still hold const Sample* into its sample map, and the pool is only
    // cleared after this, so no sample pointer outlives its owner.
    activeLoader_.store (nullptr, std::memory_order_relaxed);
    if (sf2Loader_)
        retiredLoaders_.push_back (std::move (sf2Loader_));

    // A fresh prepareToPlay() may come with the bundle now present (late
    // install) or a different font packaged, so allow the bundled font to be
    // offered again on the next audio start.
    bundledFontLoaded_ = false;

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

    const auto driveParam = apvts_.getRawParameterValue(ParamIDs::voiceDrive);
    const auto curveParam = apvts_.getRawParameterValue(ParamIDs::voiceCurve);
    const auto velToDriveParam = apvts_.getRawParameterValue(ParamIDs::voiceVelToDrive);
    const auto filterRoutingParam = apvts_.getRawParameterValue(ParamIDs::voiceFilterRouting);
    const auto filterOffsetParam = apvts_.getRawParameterValue(ParamIDs::voiceFilterOffset);
    const auto polyLimitParam = apvts_.getRawParameterValue(ParamIDs::polyLimit);
    const float driveDb = driveParam ? driveParam->load() : 0.0f;
    const int curveId = curveParam ? static_cast<int>(curveParam->load()) : 0;
    const float velToDriveDb = velToDriveParam ? velToDriveParam->load() : 0.0f;
    const int filterRouting = filterRoutingParam ? static_cast<int>(filterRoutingParam->load()) : 0;
    const float filterOffsetCents = filterOffsetParam ? filterOffsetParam->load() : 0.0f;

    if (polyLimitParam)
        voicePool_->setPolyphony (static_cast<int> (polyLimitParam->load()));

    auto renderRange = [&] (int startSample, int endSample) noexcept
    {
        const int rangeLength = endSample - startSample;
        if (rangeLength > 0)
            voicePool_->render (outL + startSample, rangeLength, static_cast<int> (sampleRate_),
                                driveDb, velToDriveDb, curveId, filterRouting, filterOffsetCents);
    };

    int renderedUntil = 0;
    for (const auto event : midiMessages)
    {
        const int eventSample = juce::jlimit (renderedUntil, numSamples, event.samplePosition);
        renderRange (renderedUntil, eventSample);
        renderedUntil = eventSample;

        const auto msg = event.getMessage();
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const int velocity = msg.getVelocity();
            const int bank = currentBank_.load (std::memory_order_relaxed);
            const int program = currentProgram_.load (std::memory_order_relaxed);
            if (Sample* sample = loader->getSample (bank, program, note, velocity))
                voicePool_->start (sample, note, static_cast<float> (velocity) / 127.0f);
        }
        else if (msg.isNoteOff())
        {
            voicePool_->stop (msg.getNoteNumber());
        }
        else if (msg.isProgramChange())
        {
            currentProgram_.store (msg.getProgramChangeNumber(), std::memory_order_relaxed);
        }
    }
    renderRange (renderedUntil, numSamples);

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
    const auto outMixParam = apvts_.getRawParameterValue(ParamIDs::outMix);
    const float outTrimDb = outTrimParam ? outTrimParam->load() : 0.0f;
    const float outTrimGain = std::pow(10.0f, outTrimDb / 20.0f);
    const float outMixGain = outMixParam ? outMixParam->load() / 100.0f : 1.0f;

    buffer.applyGain(outTrimGain * outMixGain);

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

// ---------------------------------------------------------------------------
// Bundled SoundFont
// ---------------------------------------------------------------------------
//
// At build time the packaged VST3/AU bundle is given an .sf2 alongside its
// moduleinfo.json, inside Contents/Resources. On macOS the plugin binary lives
// at <bundle>/Contents/MacOS/<name>, so the Resources sibling is found by
// walking up. The bundled font is offered during prepareToPlay() so a host that
// loads the plugin is immediately playable without a manual Load step.
namespace
{
    juce::File pathForBundledSoundFont()
    {
#if JUCE_MAC
        // Locate this module's own path with dladdr and walk up from
        // <bundle>/Contents/MacOS/<name> to <bundle>/Contents/Resources. This
        // works in every host because it does not depend on which bundle the
        // host considers "main".
        Dl_info info;
        if (dladdr (reinterpret_cast<void*> (&pathForBundledSoundFont), &info) == 0
            || info.dli_fname == nullptr)
            return {};

        juce::File resourcesDir = juce::File (juce::String (info.dli_fname))
                                      .getParentDirectory()   // Contents/MacOS
                                      .getParentDirectory()   // Contents
                                      .getChildFile ("Resources");
        for (auto& entry : juce::RangedDirectoryIterator (resourcesDir, false))
            if (entry.getFile().hasFileExtension (".sf2"))
                return entry.getFile();

        return {};
#else
        // Non-macOS packaging does not yet bundle an SF2.
        return {};
#endif
    }
} // namespace

void RomplerProcessor::loadBundledSoundFont()
{
    if (bundledFontLoaded_)
        return;

    const juce::File file = pathForBundledSoundFont();
    if (file.existsAsFile())
    {
        loadSoundFont (file);
        // Only latch once the font is actually loaded. A failed or absent
        // bundle must be retried on the next prepareToPlay() (e.g. the host
        // restarts audio, or the bundle appears after a late install).
        bundledFontLoaded_ = (sf2Loader_ != nullptr);
    }
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

    // When the queue is full we drop the *new* event rather than the oldest.
    // Dropping the oldest strands a note-on without its matching note-off (or
    // vice versa): a lost note-on leaves the note silent, but a lost note-off
    // leaves it ringing forever. Both are dropped symmetrically here, so the
    // worst case is a momentarily missed keypress, never a stuck note.
    if (noteQueue_.size() < maxQueuedNotes)
        noteQueue_.push ({ note, on, velocity });
}

} // namespace aod

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new aod::RomplerProcessor();
}
