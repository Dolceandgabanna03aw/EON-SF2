#include "PluginEditor.h"

namespace aod
{

// ============================================================================
// ParameterControl — float/int sliders
// ============================================================================

ParameterControl::ParameterControl (juce::RangedAudioParameter& param)
    : param_ (param)
{
    addAndMakeVisible (label_);
    label_.setText (param.getName (64), juce::dontSendNotification);
    label_.setJustificationType (juce::Justification::centredLeft);
    label_.setColour (juce::Label::textColourId, juce::Colours::whitesmoke);

    addAndMakeVisible (slider_);
    slider_.setSliderStyle (juce::Slider::LinearHorizontal);
    slider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);

    attachment_ = std::make_unique<juce::SliderParameterAttachment> (param, slider_);

    setSize (300, 40);
}

ParameterControl::~ParameterControl() = default;

void ParameterControl::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));
}

void ParameterControl::resized()
{
    auto bounds = getLocalBounds().reduced (4);

    label_.setBounds (bounds.removeFromLeft (100));
    bounds.removeFromLeft (4);
    slider_.setBounds (bounds);
}


// ============================================================================
// ParameterChoiceControl — choice dropdowns
// ============================================================================

ParameterChoiceControl::ParameterChoiceControl (juce::AudioParameterChoice& param)
    : param_ (param)
{
    addAndMakeVisible (label_);
    label_.setText (param.getName (64), juce::dontSendNotification);
    label_.setJustificationType (juce::Justification::centredLeft);
    label_.setColour (juce::Label::textColourId, juce::Colours::whitesmoke);

    addAndMakeVisible (box_);
    const auto& choices = param.getAllValueStrings();
    for (const auto& choice : choices)
        box_.addItem (choice, box_.getNumItems() + 1);

    attachment_ = std::make_unique<juce::ComboBoxParameterAttachment> (param, box_);

    setSize (300, 40);
}

ParameterChoiceControl::~ParameterChoiceControl() = default;

void ParameterChoiceControl::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));
}

void ParameterChoiceControl::resized()
{
    auto bounds = getLocalBounds().reduced (4);

    label_.setBounds (bounds.removeFromLeft (100));
    bounds.removeFromLeft (4);
    box_.setBounds (bounds);
}


// ============================================================================
// RomplerEditor
// ============================================================================

RomplerEditor::RomplerEditor (RomplerProcessor& processorRef)
    : juce::AudioProcessorEditor (processorRef),
      processor_ (processorRef)
{
    addAndMakeVisible (loadButton_);
    loadButton_.onClick = [this] { onLoadButtonClicked(); };

    addAndMakeVisible (fileLabel_);
    fileLabel_.setText ("No SoundFont loaded", juce::dontSendNotification);
    fileLabel_.setJustificationType (juce::Justification::centredLeft);
    fileLabel_.setColour (juce::Label::textColourId, juce::Colours::silver);

    addAndMakeVisible (presetBox_);
    presetBox_.setTextWhenNoChoicesAvailable ("(no presets)");
    presetBox_.addListener (this);

    // Build parameter controls in order: Drive, Curve, VelToDrive, FilterRouting,
    // FilterOffset, PolyLimit, TapeDrive, Fold, OsFactor, OutTrim, OutMix
    auto& apvts = processor_.getValueTreeState();

    paramControls_[0] = std::make_unique<ParameterControl> (
        *dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::voiceDrive)));
    paramControls_[1] = std::make_unique<ParameterChoiceControl> (
        *dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::voiceCurve)));
    paramControls_[2] = std::make_unique<ParameterControl> (
        *dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::voiceVelToDrive)));
    paramControls_[3] = std::make_unique<ParameterChoiceControl> (
        *dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::voiceFilterRouting)));
    paramControls_[4] = std::make_unique<ParameterControl> (
        *dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::voiceFilterOffset)));
    paramControls_[5] = std::make_unique<ParameterControl> (
        *dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::polyLimit)));
    paramControls_[6] = std::make_unique<ParameterControl> (
        *dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::busTapeDrive)));
    paramControls_[7] = std::make_unique<ParameterControl> (
        *dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::busFold)));
    paramControls_[8] = std::make_unique<ParameterChoiceControl> (
        *dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::busOsFactor)));
    paramControls_[9] = std::make_unique<ParameterControl> (
        *dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::outTrim)));
    paramControls_[10] = std::make_unique<ParameterControl> (
        *dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::outMix)));

    for (auto& ctrl : paramControls_)
        addAndMakeVisible (*ctrl);

    setSize (440, 60 + 11 * 40);
}

RomplerEditor::~RomplerEditor()
{
    presetBox_.removeListener (this);
}

void RomplerEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d0d));
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
    for (auto& ctrl : paramControls_)
    {
        if (ctrl)
            ctrl->setBounds (bounds.removeFromTop (40));
    }
}

void RomplerEditor::onLoadButtonClicked()
{
    fileChooser_ = std::make_unique<juce::FileChooser> (
        "Select a SoundFont", juce::File(), "*.sf2");

    // Not named `flags`: juce::Component carries an inherited member of that
    // name, and GCC's -Wshadow reports the collision even though it is private.
    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;

    fileChooser_->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
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
