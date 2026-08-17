#include "PluginEditor.h"

namespace aod
{

RomplerEditor::RomplerEditor (RomplerProcessor& processorRef)
    : juce::AudioProcessorEditor (processorRef),
      processor_ (processorRef)
{
    addAndMakeVisible (loadButton_);
    loadButton_.onClick = [this] { onLoadButtonClicked(); };

    addAndMakeVisible (fileLabel_);
    fileLabel_.setText ("No SoundFont loaded", juce::dontSendNotification);
    fileLabel_.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (presetBox_);
    presetBox_.setTextWhenNoChoicesAvailable ("(no presets)");
    presetBox_.addListener (this);

    parametersEditor_ = std::make_unique<juce::GenericAudioProcessorEditor> (processor_);
    addAndMakeVisible (*parametersEditor_);

    const int parametersHeight = parametersEditor_->getHeight();
    setSize (500, 80 + parametersHeight);
}

RomplerEditor::~RomplerEditor()
{
    presetBox_.removeListener (this);
}

void RomplerEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void RomplerEditor::resized()
{
    auto bounds = getLocalBounds().reduced (8);

    auto topRow = bounds.removeFromTop (28);
    loadButton_.setBounds (topRow.removeFromLeft (140));
    topRow.removeFromLeft (8);
    fileLabel_.setBounds (topRow);

    bounds.removeFromTop (8);
    presetBox_.setBounds (bounds.removeFromTop (28));

    bounds.removeFromTop (8);
    if (parametersEditor_ != nullptr)
        parametersEditor_->setBounds (bounds);
}

void RomplerEditor::onLoadButtonClicked()
{
    fileChooser_ = std::make_unique<juce::FileChooser> (
        "Select a SoundFont", juce::File(), "*.sf2");

    const auto flags = juce::FileBrowserComponent::openMode
                      | juce::FileBrowserComponent::canSelectFiles;

    fileChooser_->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        processor_.loadSoundFont (file);
        fileLabel_.setText (processor_.getLoadedFileName(), juce::dontSendNotification);
        refreshPresetList();
    });
}

void RomplerEditor::refreshPresetList()
{
    presetBox_.clear (juce::dontSendNotification);

    const int count = processor_.getPresetCount();
    for (int i = 0; i < count; ++i)
        presetBox_.addItem (processor_.getPresetName (i), i + 1);

    if (count > 0)
        presetBox_.setSelectedId (1, juce::dontSendNotification);
}

void RomplerEditor::comboBoxChanged (juce::ComboBox* box)
{
    if (box != &presetBox_)
        return;

    const int presetIndex = presetBox_.getSelectedId() - 1;
    if (presetIndex < 0)
        return;

    const auto [bank, program] = processor_.getPresetBankProgram (presetIndex);
    processor_.selectPreset (bank, program);
}

} // namespace aod
