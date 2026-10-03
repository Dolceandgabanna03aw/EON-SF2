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
    // The host guarantees the audio thread is stopped here (and a host may
    // call prepareToPlay() twice without releaseResources() in between).
    prepared_ = false;
    sampleRate_ = sampleRate;

    // A fresh pool has no voices, so nothing references any retired loader.
    voicePool_ = std::make_unique<VoicePool>();
    retiredLoaders_.clear();
    lastSeenLoader_ = nullptr;
    loaderSwitchStamp_ = 0;
    acknowledgedLoader_.store (nullptr, std::memory_order_relaxed);

    // The loaded bank survives releaseResources(). If it was resampled for a
    // different rate, rebuild it from the same file at the new rate, keeping
    // the selected preset. This runs here, not on the audio thread. If the
    // file is gone the old loader stays: Voice scales its step by
    // sampleRate / hostRate, so it still plays in tune, only with the
    // original rate conversion.
    if (sf2Loader_ != nullptr && sf2Loader_->hostSampleRate() != static_cast<int> (sampleRate))
    {
        const auto bankProgram = getCurrentBankProgram();
        if (loadSoundFontInternal (loadedFile_, &bankProgram, false))
            retiredLoaders_.clear(); // the pool is fresh: nothing reads the old one
    }

    // Surface the bundled SoundFont so the plugin starts usable without a
    // manual Load step when the packaged font is present and nothing else
    // (a user file or a restored session) is loaded.
    if (sf2Loader_ == nullptr)
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

    prepared_ = true;
}

void RomplerProcessor::releaseResources()
{
    // The audio thread is guaranteed stopped here. The loaded bank is kept,
    // not retired: the host may restart audio (sample-rate change, device
    // switch, offline bounce) and the user's SoundFont must still be there.
    // activeLoader_ stays published. processBlock() returns early without a
    // voice pool, so a host that calls it before prepareToPlay() gets
    // silence, not a dangling loader.
    //
    // The voices go first, then the retired loaders: once the pool is gone
    // no Sample* into a retired loader is left anywhere.
    prepared_ = false;
    voicePool_.reset();
    retiredLoaders_.clear();
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

    // First block on a newly published loader: every voice started up to
    // now may hold samples from an older loader. See acknowledgedLoader_.
    if (loader != lastSeenLoader_)
    {
        lastSeenLoader_ = loader;
        loaderSwitchStamp_ = voicePool_->lastStartOrder();
    }

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
    // Drive and Velocity-to-Drive are percentages; the voice wants 0..1 and
    // -1..1. (They used to be passed through as dB, so 20 % meant +20 dB of
    // pre-gain followed by a -20 dB divide.) See Voice::render for the mapping.
    const float driveAmount = driveParam ? driveParam->load() / 100.0f : 0.0f;
    const int curveId = curveParam ? static_cast<int>(curveParam->load()) : 0;
    const float velToDrive = velToDriveParam ? velToDriveParam->load() / 100.0f : 0.0f;
    const int filterRouting = filterRoutingParam ? static_cast<int>(filterRoutingParam->load()) : 0;
    const float filterOffsetCents = filterOffsetParam ? filterOffsetParam->load() : 0.0f;

    if (polyLimitParam)
        voicePool_->setPolyphony (static_cast<int> (polyLimitParam->load()));

    auto renderRange = [&] (int startSample, int endSample) noexcept
    {
        const int rangeLength = endSample - startSample;
        if (rangeLength > 0)
            voicePool_->render (outL + startSample, rangeLength, static_cast<int> (sampleRate_),
                                driveAmount, velToDrive, curveId, filterRouting, filterOffsetCents);
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

    // Once every voice from before the switch has finished, no voice can be
    // reading a retired loader. Let the message thread free them.
    if (acknowledgedLoader_.load (std::memory_order_relaxed) != loader
        && ! voicePool_->anyActiveStartedAtOrBefore (loaderSwitchStamp_))
        acknowledgedLoader_.store (loader, std::memory_order_release);

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

namespace
{
    // State properties added alongside the APVTS parameters. A state saved by
    // an older build has none of them and restores exactly as before.
    const juce::Identifier sf2PathProperty    { "sf2Path" };
    const juce::Identifier sf2BankProperty    { "sf2Bank" };
    const juce::Identifier sf2ProgramProperty { "sf2Program" };
} // namespace

void RomplerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    state.setProperty (sf2PathProperty, getSoundFontPath(), nullptr);
    const auto [bank, program] = getCurrentBankProgram();
    state.setProperty (sf2BankProperty, bank, nullptr);
    state.setProperty (sf2ProgramProperty, program, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RomplerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts_.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    const bool hasSoundFontInfo = state.hasProperty (sf2PathProperty)
                               || state.hasProperty (sf2BankProperty);
    const juce::String path = state.getProperty (sf2PathProperty).toString();
    const std::pair<int, int> bankProgram { static_cast<int> (state.getProperty (sf2BankProperty, 0)),
                                            static_cast<int> (state.getProperty (sf2ProgramProperty, 0)) };

    // Keep the APVTS tree to parameters only.
    state.removeProperty (sf2PathProperty, nullptr);
    state.removeProperty (sf2BankProperty, nullptr);
    state.removeProperty (sf2ProgramProperty, nullptr);
    apvts_.replaceState (state);

    // Older state: leave whatever bank is loaded (bundled or user) alone.
    if (! hasSoundFontInfo)
        return;

    if (path.isNotEmpty())
    {
        {
            // Remember the session's file even if it cannot be loaded right
            // now (moved drive, other machine), so re-saving keeps it.
            const std::lock_guard lock (soundFontPathMutex_);
            soundFontPath_ = path;
        }

        if (juce::File::isAbsolutePath (path))
        {
            const juce::File file (path);
            if (sf2Loader_ != nullptr && loadedFile_ == file)
            {
                selectPreset (bankProgram.first, bankProgram.second);
                return;
            }
            // Loads at the current rate; prepareToPlay() rebuilds it if the
            // host then prepares at a different one.
            if (loadSoundFontInternal (file, &bankProgram, true))
                return;
        }
    }

    // No file, or it failed to load: keep the current bank and apply the
    // preset to it, or to the bundled font once prepareToPlay() loads it.
    selectPreset (bankProgram.first, bankProgram.second);
    if (sf2Loader_ == nullptr)
        pendingBankProgram_ = bankProgram;
}

juce::String RomplerProcessor::getSoundFontPath() const
{
    const std::lock_guard lock (soundFontPathMutex_);
    return soundFontPath_;
}

void RomplerProcessor::loadSoundFont (const juce::File& file)
{
    loadSoundFontInternal (file, nullptr, true);
}

bool RomplerProcessor::loadSoundFontInternal (const juce::File& file, const std::pair<int, int>* bankProgram,
                                              bool rememberPath)
{
    auto newLoader = std::make_unique<SF2Loader>(static_cast<int>(sampleRate_));
    if (!newLoader->loadFile(file))
        return false;

    const auto [bank, program] = bankProgram != nullptr ? *bankProgram : newLoader->firstPresetProgram();
    currentBank_.store (bank, std::memory_order_relaxed);
    currentProgram_.store (program, std::memory_order_relaxed);
    loadedFileName_ = file.getFileName();
    loadedFile_ = file;
    pendingBankProgram_.reset();

    if (rememberPath)
    {
        const std::lock_guard lock (soundFontPathMutex_);
        soundFontPath_ = file.getFullPathName();
    }

    // Reclaim loaders retired by earlier swaps if the audio thread has
    // confirmed it no longer uses them. This keeps retiredLoaders_ from
    // growing without bound across repeated loads in one session.
    freeRetiredLoadersIfUnused();

    // Publish the new loader before retiring the old one: a note-on on the
    // audio thread that reads activeLoader_ right now must see either the
    // fully-built new loader or the still-valid old one, never a half state.
    activeLoader_.store (newLoader.get(), std::memory_order_release);

    if (sf2Loader_)
        retiredLoaders_.push_back (std::move (sf2Loader_));
    sf2Loader_ = std::move (newLoader);
    return true;
}

void RomplerProcessor::freeRetiredLoadersIfUnused()
{
    if (retiredLoaders_.empty())
        return;

    // Not prepared: no audio thread is running and the pool is empty or gone.
    // Prepared: safe once the audio thread has acknowledged the current
    // loader, i.e. every voice that could hold an older loader's samples has
    // finished. Retired loaders are always older than sf2Loader_.
    if (! prepared_
        || (sf2Loader_ != nullptr
            && acknowledgedLoader_.load (std::memory_order_acquire) == sf2Loader_.get()))
        retiredLoaders_.clear();
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
        // Not remembered as the session's file: the bundle is located again
        // at startup. A preset restored from state is applied to it.
        const auto pending = pendingBankProgram_;
        loadSoundFontInternal (file, pending ? &*pending : nullptr, false);
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
