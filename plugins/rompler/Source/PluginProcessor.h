#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cstdint>
#include <queue>
#include <mutex>
#include <optional>
#include <tuple>

#include "Parameters.h"
#include "Sampler.h"
#include "SF2Loader.h"
#include "BusProcessor.h"
#include "FxProcessor.h"

namespace aod
{

class RomplerProcessor final : public juce::AudioProcessor
{
public:
    using juce::AudioProcessor::processBlock;

    RomplerProcessor();
    ~RomplerProcessor() override = default;

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    [[nodiscard]] juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts_; }

    /** Peak magnitude of the most recently rendered block, for the UI meter. */
    [[nodiscard]] float getLastPeak() const noexcept { return lastPeak_.load (std::memory_order_relaxed); }

    /**
        Loads a SoundFont from disk and, on success, selects its first preset.

        Safe to call from the message thread only: it allocates and does file
        I/O. processBlock() picks up the new bank through a released/acquired
        atomic pointer swap, never touching the old unique_ptr while the audio
        thread might still be reading it.
    */
    void loadSoundFont (const juce::File& file);

    /**
        Loads the SoundFont bundled inside the plugin bundle's Contents/Resources
        directory, if one is present. Called from prepareToPlay() on the message
        thread so the plugin starts already usable without a manual Load step.
        No-op when the bundle has no .sf2 resource (e.g. release builds without
        the bundled font, the ui_shot tool, the standalone build, or local dev).
    */
    void loadBundledSoundFont();

    [[nodiscard]] juce::String getLoadedFileName() const noexcept { return loadedFileName_; }

    /**
        Full path of the user-chosen SoundFont this session refers to: the
        last file passed to loadSoundFont() or restored from saved state.
        Empty when only the bundled font (or nothing) is loaded. Kept even
        if the restored file is missing, so re-saving does not lose it.
    */
    [[nodiscard]] juce::String getSoundFontPath() const;

    /** Loaders replaced while audio may still have been reading them. Exposed for tests. */
    [[nodiscard]] std::size_t getRetiredLoaderCount() const noexcept { return retiredLoaders_.size(); }

    [[nodiscard]] int getPresetCount() const noexcept;
    [[nodiscard]] juce::String getPresetName (int presetIndex) const noexcept;
    [[nodiscard]] std::pair<int, int> getPresetBankProgram (int presetIndex) const noexcept;

    /** Selects which (bank, program) note-on resolves against. Message-thread only. */
    void selectPreset (int bank, int program) noexcept;

    /** Current (bank, program) pair, for the UI readout. Message-thread safe. */
    [[nodiscard]] std::pair<int, int> getCurrentBankProgram() const noexcept
    {
        return { currentBank_.load (std::memory_order_relaxed),
                 currentProgram_.load (std::memory_order_relaxed) };
    }

    /**
        Queues a note-on/off for the audio thread. Called from the message thread
        (UI keyboard, computer keyboard); drained inside processBlock() so it is
        sample-accurate and allocation-free on the audio thread.
    */
    void postNote (int note, bool on, int velocity = 100);

private:
    /**
        Builds a loader at the current sample rate and publishes it.
        bankProgram selects the preset afterwards (nullptr = first preset).
        rememberPath records the file as the session's SoundFont (false for the
        bundled font, which is found again at startup rather than saved).
    */
    bool loadSoundFontInternal (const juce::File& file, const std::pair<int, int>* bankProgram,
                                bool rememberPath);

    /** Frees retired loaders once the audio thread can no longer reference them. */
    void freeRetiredLoadersIfUnused();

    /** Cap on buffered message-thread note events; see postNote(). */
    static constexpr std::size_t maxQueuedNotes = 256;

    juce::AudioProcessorValueTreeState apvts_;

    // sf2Loader_ is owned and replaced only on the message thread. processBlock
    // reads activeLoader_ instead, so a load in progress never races a note-on:
    // the pointer swap is the only thing shared, and it is atomic.
    std::unique_ptr<SF2Loader> sf2Loader_;
    std::atomic<SF2Loader*> activeLoader_ { nullptr };
    std::vector<std::unique_ptr<SF2Loader>> retiredLoaders_;

    // Retired-loader reclamation. The audio thread notes the first block in
    // which it sees a new activeLoader_ (and the voice-pool start stamp at
    // that moment); once no voice started at or before that stamp is still
    // active, nothing can reference an older loader's samples, and it
    // publishes the loader in acknowledgedLoader_. The message thread then
    // frees retiredLoaders_ before the next swap. Audio-thread-only state is
    // reset in prepareToPlay(), where the audio thread is stopped.
    std::atomic<SF2Loader*> acknowledgedLoader_ { nullptr };
    SF2Loader* lastSeenLoader_ = nullptr;
    std::uint64_t loaderSwitchStamp_ = 0;
    bool prepared_ = false;

    std::unique_ptr<VoicePool> voicePool_;
    BusProcessor busProcessor_;
    FxProcessor fxProcessor_;
    int cachedOsFactorIndex_ = 2;
    double sampleRate_ = 48000.0;
    std::atomic<int> currentBank_ { 0 };
    std::atomic<int> currentProgram_ { 0 };
    std::atomic<float> lastPeak_ { 0.0f };

    /** Set once the bundled font has been offered up; see loadBundledSoundFont(). */
    bool bundledFontLoaded_ = false;

    // Message-thread -> audio-thread note events. Bounded; if the host is not
    // running we drop rather than grow unbounded.
    std::queue<std::tuple<int, bool, int>> noteQueue_;
    std::mutex noteQueueMutex_;

    juce::String loadedFileName_;

    // The file the current loader was built from (bundled or user), used to
    // rebuild it when the sample rate changes. Message thread only.
    juce::File loadedFile_;

    // Session SoundFont path for get/setStateInformation. Guarded because
    // some hosts save state from a background thread.
    mutable std::mutex soundFontPathMutex_;
    juce::String soundFontPath_;

    // Preset restored from state while no bank could be loaded yet; applied
    // when the bundled font loads in prepareToPlay(). Message thread only.
    std::optional<std::pair<int, int>> pendingBankProgram_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomplerProcessor)
};

} // namespace aod
