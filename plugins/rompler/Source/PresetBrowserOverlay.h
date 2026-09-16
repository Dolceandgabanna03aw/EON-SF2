#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PresetLibrary.h"

namespace aod
{

/** Opaque, non-modal preset browser which keeps all loading decisions in the editor. */
class PresetBrowserOverlay final : public juce::Component,
                                   private juce::ListBoxModel
{
public:
    enum class DirtyChoice { save, discard, cancel };

    explicit PresetBrowserOverlay (PresetLibrary& library);
    ~PresetBrowserOverlay() override;

    void refresh();
    void setCurrentUuid (const juce::String& uuid);
    void setDirty (bool isDirty);

    std::function<void (juce::String)> onLoadRequested;
    std::function<void (juce::String, DirtyChoice)> onDirtyChoice;
    std::function<void()> onCloseRequested;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics&, int width,
                           int height, bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowChanged) override;
    void applyQuery();
    void invokeSelected();
    void chooseDirty (DirtyChoice choice);

    PresetLibrary& library_;
    juce::TextEditor searchEditor_;
    juce::ComboBox categoryBox_;
    juce::ToggleButton favouritesButton_ { "FAVOURITES" };
    juce::ListBox list_ { "preset browser", this };
    juce::TextButton upButton_ { "UP" };
    juce::TextButton downButton_ { "DOWN" };
    juce::TextButton enterButton_ { "LOAD" };
    juce::TextButton escapeButton_ { "CLOSE" };
    juce::TextButton saveButton_ { "SAVE" };
    juce::TextButton discardButton_ { "DISCARD" };
    juce::TextButton cancelButton_ { "CANCEL" };
    juce::Label title_;
    juce::Label dirtyPrompt_;
    std::vector<const PresetEntryModel*> rows_;
    juce::String currentUuid_;
    bool dirty_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowserOverlay)
};

} // namespace aod
