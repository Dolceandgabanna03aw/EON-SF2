#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "Sampler.h"
#include "SF2Loader.h"

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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    [[nodiscard]] juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts_; }

    void loadSoundFont(const juce::File& file);

private:
    juce::AudioProcessorValueTreeState apvts_;
    std::unique_ptr<SF2Loader> sf2Loader_;
    std::unique_ptr<VoicePool> voicePool_;
    double sampleRate_ = 48000.0;
    int currentBank_ = 0;
    int currentProgram_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomplerProcessor)
};

} // namespace aod
