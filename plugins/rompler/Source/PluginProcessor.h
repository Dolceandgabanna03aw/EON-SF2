#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

#include "Parameters.h"
#include "Sampler.h"
#include "SF2Loader.h"

namespace eon
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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    [[nodiscard]] juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts_; }

    /** Returns false if the file could not be parsed, in which case the bank
        that was already loaded is left in place. Message thread only. */
    bool loadSoundFont(const juce::File& file);

    /** Name of the bank currently loaded, or an empty string. Message thread only. */
    [[nodiscard]] juce::String getLoadedFileName() const { return loadedFileName_; }

    // Named for MIDI deliberately: getCurrentProgram() is already taken by
    // AudioProcessor for the host's program slot, and an overload of it here
    // silently resolves to that one — which returns a constant 0.
    [[nodiscard]] int getMidiBank() const noexcept
    {
        return currentBank_.load (std::memory_order_relaxed);
    }

    [[nodiscard]] int getMidiProgram() const noexcept
    {
        return currentProgram_.load (std::memory_order_relaxed);
    }

    /** Post-trim output peak of the last block, for the editor's meter. */
    [[nodiscard]] float getPeakLevel() const noexcept
    {
        return peakLevel_.load (std::memory_order_relaxed);
    }

private:
    juce::AudioProcessorValueTreeState apvts_;
    std::unique_ptr<SF2Loader> sf2Loader_;
    std::unique_ptr<VoicePool> voicePool_;
    double sampleRate_ = 48000.0;

    // Written by loadSoundFont on the message thread and by processBlock on the
    // audio thread (program change), so neither can be a plain int.
    std::atomic<int> currentBank_ { 0 };
    std::atomic<int> currentProgram_ { 0 };
    std::atomic<float> peakLevel_ { 0.0f };

    juce::String loadedFileName_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomplerProcessor)
};

} // namespace eon
