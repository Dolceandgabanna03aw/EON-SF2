#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <array>
#include <vector>
#include <functional>

#include "PluginProcessor.h"
#include "Parameters.h"

namespace aod
{

// Restrained studio palette: sumi ink chassis, warm hanji panels, celadon jade
// indicators, and cinnabar lacquer reserved for drive shaping and the maker's seal.
namespace theme
{
    inline juce::Colour body          { 0xff181a18 };
    inline juce::Colour body2         { 0xff121412 };
    inline juce::Colour bodyEdge      { 0xff2c302a };
    inline juce::Colour chassisRule   { 0xff363b33 };
    inline juce::Colour panel         { 0xffded7c4 };
    inline juce::Colour panelSubtle   { 0xffd0c8b2 };
    inline juce::Colour panelEdge     { 0xffa69e89 };
    inline juce::Colour panelRule     { 0xffbeb6a0 };
    inline juce::Colour ink           { 0xff1f1d19 };
    inline juce::Colour inkSoft       { 0xff5e584a };
    inline juce::Colour inkMuted      { 0xff858074 };
    inline juce::Colour inkLight      { 0xffebe5d5 };
    inline juce::Colour jade          { 0xff84b8a3 };
    inline juce::Colour jadeDeep      { 0xff4b7c6a };
    inline juce::Colour jadeWash      { 0xff1b2b25 };
    inline juce::Colour cinnabar      { 0xffc66a43 };
    inline juce::Colour cinnabarDeep  { 0xff914628 };
    inline juce::Colour sealRed       { 0xffa83e32 };
    inline juce::Colour knobCap       { 0xfff3eee0 };
    inline juce::Colour knobShadow    { 0xffc6beaa };
    inline juce::Colour ledOff        { 0xff222723 };
    inline juce::Colour displayBg     { 0xff0f1512 };
    inline juce::Colour displayEdge   { 0xff27332d };
    inline juce::Colour keyFelt       { 0xff6e2c24 };
}

// Build a font at a given point height, optionally bold.
inline juce::Font makeFont (float pt, bool bold)
{
    return bold ? juce::Font (juce::FontOptions (pt, juce::Font::bold))
                : juce::Font (juce::FontOptions (pt));
}

/**
    A warm hanji-stone panel with an integrated hairline header bar.
    Child controls are laid out by the owning editor.
*/
class SectionBox final : public juce::Component
{
public:
    explicit SectionBox (const juce::String& title,
                         const juce::String& subtitle = {},
                         bool splitMid = false);
    ~SectionBox() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label title_;
    juce::Label subtitle_;
    bool splitMid_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionBox)
};

/**
    Rotary control drawn as a cream knob with a mint pointer, styled for the
    synth-deck panel. Drives a single RangedAudioParameter through a
    SliderParameterAttachment. Vertical drag and the mouse wheel change the
    value; the name and live value print below.
*/
class Knob final : public juce::Component,
                   private juce::AudioProcessorParameter::Listener,
                   private juce::AsyncUpdater
{
public:
    explicit Knob (juce::RangedAudioParameter& param, bool hot = false, bool bipolar = false);
    ~Knob() override;

    /** Optionally override the printed name with an explicit UI label. */
    void setNameOverride (const juce::String& label) { name_.setText (label, juce::dontSendNotification); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Called (synchronously, possibly from the audio thread) when the bound
        parameter's value changes from any source - drag, wheel, host
        automation, preset restore. We only flag an async update here; the
        actual repaint happens on the message thread in handleAsyncUpdate. */
    void parameterValueChanged (int, float) override;
    void parameterGestureChanged (int, bool) override {}

private:
    void handleAsyncUpdate() override;

    [[maybe_unused]] juce::RangedAudioParameter& param_;
    juce::Slider slider_;
    juce::Label name_;
    juce::Label value_;
    std::unique_ptr<juce::SliderParameterAttachment> attachment_;
    float lastDragY_ = 0.0f;
    bool hot_ = false;
    bool bipolar_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

/**
    A labelled segmented selector for choice parameters. Drives an AudioParameterChoice via
    a ComboBoxParameterAttachment backed by a hidden combo box.
*/
class Switch final : public juce::Component
{
public:
    explicit Switch (juce::AudioParameterChoice& param, const juce::String& label = {}, bool leds = false);
    ~Switch() override;

    void setLabel (const juce::String& s) { label_.setText (s, juce::dontSendNotification); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void advanceChoice();

    [[maybe_unused]] juce::AudioParameterChoice& param_;
    juce::Label label_;
    juce::ComboBox box_;
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment_;
    juce::Rectangle<int> pill_;
    bool leds_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Switch)
};

/**
    A 2-way segmented rocker for binary choice parameters (Pre / Post).
*/
class Toggle final : public juce::Component
{
public:
    explicit Toggle (juce::AudioParameterChoice& param, const juce::String& label = {});
    ~Toggle() override;

    void setLabel (const juce::String& s) { label_.setText (s, juce::dontSendNotification); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    [[maybe_unused]] juce::AudioParameterChoice& param_;
    juce::Label label_;
    juce::ComboBox box_;
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment_;
    juce::Rectangle<int> pill_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Toggle)
};

/**
    A numeric stepper with -/+ zones and a dark readout. Drives an AudioParameterInt via a
    SliderParameterAttachment backed by a hidden slider.
*/
class Stepper final : public juce::Component,
                      private juce::AudioProcessorParameter::Listener,
                      private juce::AsyncUpdater
{
public:
    explicit Stepper (juce::RangedAudioParameter& param, const juce::String& label = {});
    ~Stepper() override;

    void setLabel (const juce::String& s) { label_.setText (s, juce::dontSendNotification); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Called (synchronously, possibly from the audio thread) when the bound
        parameter's value changes from any source. Only flags an async update;
        the digit readout repaints on the message thread in handleAsyncUpdate. */
    void parameterValueChanged (int, float) override;
    void parameterGestureChanged (int, bool) override {}

private:
    void handleAsyncUpdate() override;

    [[maybe_unused]] juce::RangedAudioParameter& param_;
    juce::Label label_;
    juce::Slider slider_;
    std::unique_ptr<juce::SliderParameterAttachment> attachment_;
    juce::Rectangle<int> readoutRect_;
    float lastDragY_ = 0.0f;
    bool dragged_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Stepper)
};

/** A compact segmented meter showing live output peak level. */
class PeakMeter final : public juce::Component
{
public:
    PeakMeter();
    ~PeakMeter() override;

    void setLevel (float level);   // 0..1
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int numSegments = 8;
    float level_ = 0.0f;
    juce::Label label_;
    juce::Rectangle<int> barRect_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PeakMeter)
};

struct PianoKey
{
    int note = 0;
    bool black = false;
    int whiteIndex = 0;   // white key this black key sits immediately to the right of
};

/** A clickable, playable piano. Sends note-on/off to the processor. */
class Keyboard final : public juce::Component
{
public:
    Keyboard();
    ~Keyboard() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setKeyRange (int lowNote, int numKeys);
    void setNoteOn (int note, bool on);
    void setNoteCallback (std::function<void (int, bool)> cb) { noteCallback_ = std::move (cb); }
    int findNote (juce::Point<float>) const;

private:
    juce::Rectangle<int> boundsFor (const PianoKey&) const;

    std::vector<PianoKey> keys_;
    int numWhite_ = 0;
    juce::HashMap<int, bool> lit_;
    int activeMouseNote_ = -1;
    std::function<void (int, bool)> noteCallback_;   // set by the editor -> postNote

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Keyboard)
};

/** A single SoundFont program as listed in the browser. */
struct PresetEntry
{
    int bank = 0;
    int program = 0;
    juce::String name;
};

/**
    The bank/program browser: a bank selector strip on top and a scrolling list
    of the programs in the active bank below. Clicking a row selects that
    program through the callback wired to RomplerProcessor::selectPreset.
*/
class BankBrowser final : public juce::Component,
                          private juce::ListBoxModel
{
public:
    BankBrowser();
    ~BankBrowser() override;

    void setPresets (std::vector<PresetEntry> presets);
    void setCurrent (int bank, int program);
    void setOnSelect (std::function<void (int, int)> cb) { onSelect_ = std::move (cb); }

    void resized() override;

private:
    // juce::ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics&, int width,
                           int height, bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowChanged) override;

    void buildBanks();
    void filterFor (int bank);

    juce::ComboBox bankCombo_;
    juce::ListBox list_ { "preset list", this };
    juce::Label emptyHint_;
    std::vector<PresetEntry> presets_;
    std::vector<int> banks_;
    std::vector<int> filtered_;          // indices into presets_ for the active bank
    int selectedBank_ = -1;
    std::function<void (int, int)> onSelect_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BankBrowser)
};

class RomplerEditor final : public juce::AudioProcessorEditor,
                            private juce::ComboBox::Listener,
                            private juce::Timer
{
public:
    explicit RomplerEditor (RomplerProcessor& processorRef);
    ~RomplerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RomplerProcessor& processor_;

    juce::Label brandTitle_;
    juce::Label brandSub_;
    juce::Label badges_;

    SectionBox voiceBox_;
    SectionBox busBox_;
    SectionBox fxBox_;
    juce::Rectangle<int> libraryRect_;

    // Controls, in a flat list. Order: VOICE (0-5), BUS (6-10), FX (11-16).
    std::array<std::unique_ptr<juce::Component>, 17> controls_;

    juce::Label sfLabel_;
    juce::Label sfDisplay_;
    juce::Label bankDigits_;
    juce::TextButton loadButton_ { "LOAD" };
    PeakMeter peakMeter_;

    std::unique_ptr<BankBrowser> bankBrowser_;
    Keyboard keyboard_;

    std::unique_ptr<juce::LookAndFeel> lnf_;
    std::unique_ptr<juce::FileChooser> fileChooser_;
    juce::String lastLoadedFile_;

    void refreshDisplay();
    void refreshPresetList();
    void updateBankDigits();
    void onLoadButtonClicked();
    void layoutVoiceControls (juce::Rectangle<int> area);
    void layoutBusControls (juce::Rectangle<int> area);
    void layoutFxControls (juce::Rectangle<int> area);

    void comboBoxChanged (juce::ComboBox*) override;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomplerEditor)
};

} // namespace aod
