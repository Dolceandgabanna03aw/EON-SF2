#include "PresetBrowserOverlay.h"

#include <algorithm>

namespace aod
{

namespace
{
const juce::Colour overlayBackground { 0xf2151e27 };
const juce::Colour overlayPanel { 0xff313e4a };
const juce::Colour overlayText { 0xfffbf8e9 };
const juce::Colour overlayMuted { 0xffb3c2cc };
const juce::Colour overlayAccent { 0xff5ad2e6 };
}

PresetBrowserOverlay::PresetBrowserOverlay (PresetLibrary& library)
    : library_ (library)
{
    setOpaque (true);
    setInterceptsMouseClicks (true, true);

    title_.setText ("PRESET LIBRARY", juce::dontSendNotification);
    title_.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    title_.setColour (juce::Label::textColourId, overlayText);
    addAndMakeVisible (title_);

    searchEditor_.setTextToShowWhenEmpty ("SEARCH PRESETS", overlayMuted);
    searchEditor_.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff151e27));
    searchEditor_.setColour (juce::TextEditor::textColourId, overlayText);
    searchEditor_.onTextChange = [this] { applyQuery(); };
    addAndMakeVisible (searchEditor_);

    categoryBox_.addItem ("ALL CATEGORIES", 1);
    categoryBox_.onChange = [this] { applyQuery(); };
    addAndMakeVisible (categoryBox_);

    favouritesButton_.setClickingTogglesState (true);
    favouritesButton_.onClick = [this] { applyQuery(); };
    addAndMakeVisible (favouritesButton_);

    list_.setRowHeight (36);
    list_.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff171f28));
    list_.setColour (juce::ListBox::outlineColourId, overlayAccent.withAlpha (0.35f));
    addAndMakeVisible (list_);

    upButton_.onClick = [this]
    {
        list_.selectRow (juce::jmax (0, list_.getSelectedRow() - 1), true);
    };
    downButton_.onClick = [this]
    {
        list_.selectRow (juce::jmin (getNumRows() - 1, list_.getSelectedRow() < 0 ? 0 : list_.getSelectedRow() + 1), true);
    };
    enterButton_.onClick = [this] { invokeSelected(); };
    escapeButton_.onClick = [this]
    {
        if (onCloseRequested)
            onCloseRequested();
    };
    saveButton_.onClick = [this] { chooseDirty (DirtyChoice::save); };
    discardButton_.onClick = [this] { chooseDirty (DirtyChoice::discard); };
    cancelButton_.onClick = [this] { chooseDirty (DirtyChoice::cancel); };
    for (auto* button : { &upButton_, &downButton_, &enterButton_, &escapeButton_,
                          &saveButton_, &discardButton_, &cancelButton_ })
        addAndMakeVisible (*button);

    dirtyPrompt_.setText ("UNSAVED CHANGES", juce::dontSendNotification);
    dirtyPrompt_.setColour (juce::Label::textColourId, overlayText);
    addAndMakeVisible (dirtyPrompt_);
    setDirty (false);
    refresh();
}

PresetBrowserOverlay::~PresetBrowserOverlay() = default;

void PresetBrowserOverlay::refresh()
{
    categoryBox_.clear (juce::dontSendNotification);
    categoryBox_.addItem ("ALL CATEGORIES", 1);
    int itemId = 2;
    for (const auto& category : library_.categories())
        categoryBox_.addItem (category, itemId++);
    categoryBox_.setSelectedId (1, juce::dontSendNotification);
    applyQuery();
}

void PresetBrowserOverlay::setCurrentUuid (const juce::String& uuid)
{
    currentUuid_ = uuid;
    const auto it = std::find_if (rows_.begin(), rows_.end(),
                                  [&uuid] (const auto* row) { return row->document.uuid == uuid; });
    if (it != rows_.end())
        list_.selectRow (static_cast<int> (std::distance (rows_.begin(), it)), false);
}

void PresetBrowserOverlay::setDirty (bool isDirty)
{
    dirty_ = isDirty;
    dirtyPrompt_.setVisible (dirty_);
    saveButton_.setVisible (dirty_);
    discardButton_.setVisible (dirty_);
    cancelButton_.setVisible (dirty_);
    resized();
}

void PresetBrowserOverlay::applyQuery()
{
    PresetQuery query;
    query.text = searchEditor_.getText();
    query.favouritesOnly = favouritesButton_.getToggleState();
    const int selected = categoryBox_.getSelectedId();
    if (selected > 1)
        query.category = categoryBox_.getText();
    rows_ = library_.search (query);
    list_.updateContent();
    setCurrentUuid (currentUuid_);
    if (list_.getSelectedRow() < 0 && !rows_.empty())
        list_.selectRow (0, false);
    repaint();
}

int PresetBrowserOverlay::getNumRows()
{
    return static_cast<int> (rows_.size());
}

void PresetBrowserOverlay::paintListBoxItem (int rowNumber, juce::Graphics& g, int width,
                                             int height, bool rowIsSelected)
{
    juce::ignoreUnused (height);
    if (rowNumber < 0 || rowNumber >= getNumRows())
        return;
    const auto& row = *rows_[static_cast<std::size_t> (rowNumber)];
    g.fillAll (rowIsSelected ? overlayAccent.withAlpha (0.22f) : juce::Colour (0xff171f28));
    g.setColour (overlayText);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (row.document.name.isEmpty() ? "UNTITLED" : row.document.name,
                12, 3, width - 120, 18, juce::Justification::centredLeft, true);
    g.setColour (overlayMuted);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    const auto detail = row.document.category + "  ·  " +
                        juce::String (row.document.bank) + ":" + juce::String (row.document.program) +
                        (row.document.source == PresetSource::factory ? "  FACTORY" : "  USER") +
                        (row.soundFontMissing ? "  ·  RELINK" : "");
    g.drawText (detail, 12, 20, width - 24, 13, juce::Justification::centredLeft, true);
    if (row.isFavourite)
    {
        g.setColour (overlayAccent);
        g.drawText ("★", width - 36, 8, 24, 18, juce::Justification::centred);
    }
}

void PresetBrowserOverlay::selectedRowsChanged (int lastRowChanged)
{
    if (lastRowChanged >= 0 && lastRowChanged < getNumRows())
        currentUuid_ = rows_[static_cast<std::size_t> (lastRowChanged)]->document.uuid;
}

void PresetBrowserOverlay::invokeSelected()
{
    const int row = list_.getSelectedRow();
    if (row < 0 || row >= getNumRows())
        return;
    const auto uuid = rows_[static_cast<std::size_t> (row)]->document.uuid;
    if (dirty_)
    {
        dirtyPrompt_.setVisible (true);
        currentUuid_ = uuid;
        return;
    }
    if (onLoadRequested)
        onLoadRequested (uuid);
}

void PresetBrowserOverlay::chooseDirty (DirtyChoice choice)
{
    const auto uuid = currentUuid_;
    if (onDirtyChoice)
        onDirtyChoice (uuid, choice);
}

bool PresetBrowserOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onCloseRequested)
            onCloseRequested();
        return true;
    }
    if (key == juce::KeyPress::returnKey)
    {
        invokeSelected();
        return true;
    }
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        const int delta = key == juce::KeyPress::upKey ? -1 : 1;
        const auto selected = list_.getSelectedRow() < 0 ? 0 : list_.getSelectedRow() + delta;
        list_.selectRow (juce::jlimit (0, juce::jmax (0, getNumRows() - 1), selected), true);
        return true;
    }
    return false;
}

void PresetBrowserOverlay::paint (juce::Graphics& g)
{
    g.fillAll (overlayBackground);
    const auto panel = getLocalBounds().toFloat().reduced (24.0f);
    g.setColour (juce::Colour (0x8a000000));
    g.fillRoundedRectangle (panel.translated (0.0f, 4.0f), 12.0f);
    g.setColour (overlayPanel);
    g.fillRoundedRectangle (panel, 12.0f);
    g.setColour (overlayAccent.withAlpha (0.45f));
    g.drawRoundedRectangle (panel.reduced (0.5f), 12.0f, 1.0f);
}

void PresetBrowserOverlay::resized()
{
    auto area = getLocalBounds().reduced (42);
    title_.setBounds (area.removeFromTop (30));
    auto filters = area.removeFromTop (34);
    searchEditor_.setBounds (filters.removeFromLeft (250));
    filters.removeFromLeft (8);
    categoryBox_.setBounds (filters.removeFromLeft (190));
    filters.removeFromLeft (8);
    favouritesButton_.setBounds (filters.removeFromLeft (130));
    area.removeFromTop (10);
    auto footer = area.removeFromBottom (36);
    list_.setBounds (area);
    upButton_.setBounds (footer.removeFromLeft (62));
    footer.removeFromLeft (6);
    downButton_.setBounds (footer.removeFromLeft (70));
    footer.removeFromLeft (6);
    enterButton_.setBounds (footer.removeFromLeft (82));
    footer.removeFromLeft (6);
    escapeButton_.setBounds (footer.removeFromLeft (82));
    if (dirty_)
    {
        dirtyPrompt_.setBounds (footer.removeFromLeft (140));
        footer.removeFromLeft (6);
        saveButton_.setBounds (footer.removeFromLeft (70));
        footer.removeFromLeft (6);
        discardButton_.setBounds (footer.removeFromLeft (88));
        footer.removeFromLeft (6);
        cancelButton_.setBounds (footer.removeFromLeft (74));
    }
}

} // namespace aod
