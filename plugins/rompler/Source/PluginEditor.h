#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <array>

#include "PluginProcessor.h"
#include "Parameters.h"

namespace aod
{

class ParameterControl final : public juce::Component
{
public:
    ParameterControl (juce::RangedAudioParameter& param);
    ~ParameterControl() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    [[maybe_unused]] juce::RangedAudioParameter& param_;
    juce::Label label_;
    juce::Slider slider_;
    std::unique_ptr<juce::SliderParameterAttachment> attachment_;
};

class ParameterChoiceControl final : public juce::Component
{
public:
    ParameterChoiceControl (juce::AudioParameterChoice& param);
    ~ParameterChoiceControl() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    [[maybe_unused]] juce::AudioParameterChoice& param_;
    juce::Label label_;
    juce::ComboBox box_;
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment_;
};

class RomplerEditor final : public juce::AudioProcessorEditor,
                             private juce::ComboBox::Listener
{
public:
    explicit RomplerEditor (RomplerProcessor& processorRef);
    ~RomplerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RomplerProcessor& processor_;

    juce::TextButton loadButton_ { "Load SoundFont..." };
    juce::Label fileLabel_;
    juce::ComboBox presetBox_;

    std::array<std::unique_ptr<juce::Component>, 11> paramControls_;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    void refreshPresetList();
    void comboBoxChanged (juce::ComboBox*) override;
    void onLoadButtonClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomplerEditor)
};

} // namespace aod
