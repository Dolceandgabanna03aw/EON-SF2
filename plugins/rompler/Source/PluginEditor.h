#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

#include "PluginProcessor.h"

namespace aod
{

/**
    M1 UI: load a SoundFont, pick a preset, and reach every APVTS parameter
    through the generic editor underneath.

    Nothing here touches the audio thread directly. The "Load..." button opens
    an async FileChooser and hands the chosen file to
    RomplerProcessor::loadSoundFont(), which does its own thread-safe publish.
*/
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
    std::unique_ptr<juce::GenericAudioProcessorEditor> parametersEditor_;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    void refreshPresetList();
    void comboBoxChanged (juce::ComboBox*) override;
    void onLoadButtonClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomplerEditor)
};

} // namespace aod
