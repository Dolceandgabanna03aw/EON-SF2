#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <queue>
#include <mutex>
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

    [[nodiscard]] juce::String getLoadedFileName() const noexcept { return loadedFileName_; }

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
    juce::AudioProcessorValueTreeState apvts_;

    // sf2Loader_ is owned and replaced only on the message thread. processBlock
    // reads activeLoader_ instead, so a load in progress never races a note-on:
    // the pointer swap is the only thing shared, and it is atomic.
    std::unique_ptr<SF2Loader> sf2Loader_;
    std::atomic<SF2Loader*> activeLoader_ { nullptr };
    std::vector<std::unique_ptr<SF2Loader>> retiredLoaders_;

    std::unique_ptr<VoicePool> voicePool_;
    BusProcessor busProcessor_;
    FxProcessor fxProcessor_;
    int cachedOsFactorIndex_ = 2;
    double sampleRate_ = 48000.0;
    std::atomic<int> currentBank_ { 0 };
    std::atomic<int> currentProgram_ { 0 };
    std::atomic<float> lastPeak_ { 0.0f };

    // Message-thread -> audio-thread note events. Bounded; if the host is not
    // running we drop rather than grow unbounded.
    std::queue<std::tuple<int, bool, int>> noteQueue_;
    std::mutex noteQueueMutex_;

    juce::String loadedFileName_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomplerProcessor)
};

} // namespace aod
