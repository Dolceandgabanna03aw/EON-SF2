#include "PluginEditor.h"

namespace eon
{

namespace
{
constexpr int kWidth  = 880;
constexpr int kHeight = 452;

constexpr int kMargin      = 22;
constexpr int kGroupGap    = 12;
constexpr int kCellColumns = 3;
constexpr int kCellRows    = 2;
} // namespace

RomplerEditor::RomplerEditor (RomplerProcessor& owner)
    : juce::AudioProcessorEditor (owner),
      processor_ (owner),
      drive_        (owner.getValueTreeState(), ParamIDs::voiceDrive,         "Drive",         true),
      velToDrive_   (owner.getValueTreeState(), ParamIDs::voiceVelToDrive,    "Vel > Drive",   true),
      filterOffset_ (owner.getValueTreeState(), ParamIDs::voiceFilterOffset,  "Filter Offset", false),
      curve_        (owner.getValueTreeState(), ParamIDs::voiceCurve,         "Curve",         { "Tanh", "Tube", "Xfrm" }),
      filterRouting_(owner.getValueTreeState(), ParamIDs::voiceFilterRouting, "Filter Route",  { "Pre", "Post" }),
      polyphony_    (owner.getValueTreeState(), ParamIDs::polyLimit,          "Polyphony"),
      tapeDrive_    (owner.getValueTreeState(), ParamIDs::busTapeDrive,       "Tape Drive",    true),
      fold_         (owner.getValueTreeState(), ParamIDs::busFold,            "Fold",          true),
      outTrim_      (owner.getValueTreeState(), ParamIDs::outTrim,            "Out Trim",      false, 1),
      mix_          (owner.getValueTreeState(), ParamIDs::outMix,             "Mix",           false),
      oversampling_ (owner.getValueTreeState(), ParamIDs::busOsFactor,        "Oversample",    { "1x", "2x", "4x", "8x" }),
      meter_        ([&owner] { return owner.getPeakLevel(); })
{
    setLookAndFeel (&lookAndFeel_);

    voiceGroup_.title = "Voice";
    voiceGroup_.cells = { &drive_, &curve_, &velToDrive_, &filterRouting_, &filterOffset_, &polyphony_ };

    busGroup_.title = "Bus";
    busGroup_.cells = { &tapeDrive_, &fold_, &oversampling_, &outTrim_, &mix_, &meter_ };

    for (auto* group : { &voiceGroup_, &busGroup_ })
        for (auto* cell : group->cells)
            addAndMakeVisible (cell);

    loadButton_.setColour (juce::TextButton::buttonColourId, theme::mintDeep);
    loadButton_.setColour (juce::TextButton::textColourOffId, theme::onMint);
    loadButton_.onClick = [this] { openSoundFontChooser(); };
    addAndMakeVisible (loadButton_);

    refreshBankReadout();
    setSize (kWidth, kHeight);
}

RomplerEditor::~RomplerEditor()
{
    setLookAndFeel (nullptr);
}

void RomplerEditor::refreshBankReadout()
{
    const auto loaded = processor_.getLoadedFileName();
    bankName_ = loaded.isEmpty() ? juce::String ("NO FILE LOADED") : loaded.toUpperCase();
}

void RomplerEditor::openSoundFontChooser()
{
    chooser_ = std::make_unique<juce::FileChooser> ("Open a SoundFont bank",
                                                    juce::File{},
                                                    "*.sf2");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    chooser_->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (file == juce::File{})
            return;

        if (processor_.loadSoundFont (file))
            refreshBankReadout();
        else
            bankName_ = "LOAD FAILED: " + file.getFileName().toUpperCase();

        repaint();
    });
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void RomplerEditor::resized()
{
    auto area = getLocalBounds().reduced (kMargin, 0);

    area.removeFromTop (kMargin);
    nameplateBounds_ = area.removeFromTop (54);

    area.removeFromTop (kGroupGap);
    dockBounds_ = area.removeFromBottom (62);
    area.removeFromBottom (kGroupGap);

    const int groupWidth = (area.getWidth() - kGroupGap) / 2;
    voiceGroup_.bounds = area.removeFromLeft (groupWidth);
    area.removeFromLeft (kGroupGap);
    busGroup_.bounds = area;

    layOutCells (voiceGroup_);
    layOutCells (busGroup_);

    auto dock = dockBounds_.reduced (16, 12);
    loadButton_.setBounds (dock.removeFromRight (104).withSizeKeepingCentre (104, 36));
}

void RomplerEditor::layOutCells (const Group& group)
{
    auto grid = group.bounds.reduced (10, 0).withTrimmedTop (18).withTrimmedBottom (8);

    const int cellWidth  = grid.getWidth() / kCellColumns;
    const int cellHeight = grid.getHeight() / kCellRows;

    for (size_t i = 0; i < group.cells.size(); ++i)
    {
        const int column = static_cast<int> (i) % kCellColumns;
        const int row    = static_cast<int> (i) / kCellColumns;

        group.cells[i]->setBounds (grid.getX() + column * cellWidth,
                                   grid.getY() + row * cellHeight,
                                   cellWidth,
                                   cellHeight);
    }
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void RomplerEditor::paintGroup (juce::Graphics& g, const Group& group) const
{
    const auto box = group.bounds.toFloat();

    g.setColour (theme::panel);
    g.fillRoundedRectangle (box, 8.0f);

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (box.reduced (1.5f), 7.0f, 1.0f);

    g.setColour (theme::mintDeep);
    g.drawRoundedRectangle (box.reduced (0.75f), 8.0f, 1.5f);

    // Tab label sitting over the top edge, the way a silkscreened section
    // heading interrupts the box rule on the hardware.
    const auto font = theme::labelFont (9.5f);
    const int textWidth = juce::GlyphArrangement::getStringWidthInt (font, group.title.toUpperCase())
                        + static_cast<int> (group.title.length()) * 2 + 16;

    const auto tab = juce::Rectangle<int> (group.bounds.getX() + 12,
                                           group.bounds.getY() - 8,
                                           textWidth, 17);

    g.setColour (theme::mintDeep);
    g.fillRoundedRectangle (tab.toFloat(), 3.0f);

    g.setColour (theme::onMint);
    theme::drawTrackedText (g, group.title.toUpperCase(), tab, font, 2.0f);
}

void RomplerEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Chassis
    g.setGradientFill (juce::ColourGradient (theme::body, bounds.getCentreX(), bounds.getY(),
                                             theme::bodyDark, bounds.getCentreX(), bounds.getBottom(),
                                             false));
    g.fillRoundedRectangle (bounds, 14.0f);

    g.setColour (theme::bodyEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 14.0f, 1.0f);

    // Mint pinstripe under the top edge
    const auto stripe = juce::Rectangle<float> (bounds.getX() + kMargin, bounds.getY() + 14.0f,
                                                bounds.getWidth() - kMargin * 2.0f, 2.0f);
    g.setColour (theme::mint.withAlpha (0.35f));
    g.fillRoundedRectangle (stripe.expanded (0.0f, 2.0f), 2.0f);
    g.setColour (theme::mint);
    g.fillRoundedRectangle (stripe, 1.0f);

    // Nameplate: a lit window rather than a stamped wordmark
    auto plate = nameplateBounds_;
    auto badges = plate.removeFromRight (200);

    const auto lcdFontLarge = theme::lcdFont (26.0f, true);
    const int lcdWidth = juce::GlyphArrangement::getStringWidthInt (lcdFontLarge, "EON-50") + 34;

    auto well = plate.removeFromLeft (juce::jmax (lcdWidth, 230)).reduced (0, 2);
    theme::drawLcdWell (g, well.toFloat(), 4.0f);

    auto wellInner = well.reduced (14, 6);
    g.setColour (theme::mint);
    g.setFont (lcdFontLarge);
    g.drawText ("EON-50", wellInner.removeFromTop (28), juce::Justification::centredLeft, false);

    g.setColour (theme::mint.withAlpha (0.65f));
    theme::drawTrackedText (g, "SF2 ROMPLER - NONLINEAR DRIVE", wellInner,
                            theme::lcdFont (8.5f), 1.0f, juce::Justification::left);

    g.setColour (theme::mint);
    theme::drawTrackedText (g, "VST3 - AU - STANDALONE", badges.removeFromTop (26).withTrimmedTop (6),
                            theme::labelFont (9.5f), 1.6f, juce::Justification::right);

    g.setColour (theme::chassisText);
    theme::drawTrackedText (g, "SISYPHUS AUDIO - M1", badges.removeFromTop (16),
                            theme::labelFont (9.5f, false), 1.6f, juce::Justification::right);

    paintGroup (g, voiceGroup_);
    paintGroup (g, busGroup_);

    // Dock
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRoundedRectangle (dockBounds_.toFloat(), 8.0f);
    g.setColour (theme::bodyEdge);
    g.drawRoundedRectangle (dockBounds_.toFloat().reduced (0.5f), 8.0f, 1.0f);

    auto dock = dockBounds_.reduced (16, 12);
    dock.removeFromRight (104 + 16);

    auto bankField = dock.removeFromLeft (juce::jmax (240, dock.getWidth() - 190));

    g.setColour (theme::chassisText);
    theme::drawTrackedText (g, "SOUNDFONT", bankField.removeFromTop (13),
                            theme::labelFont (9.5f), 1.4f, juce::Justification::left);

    theme::drawLcdWell (g, bankField.toFloat());
    g.setColour (theme::mint);
    g.setFont (theme::lcdFont (12.5f));
    g.drawFittedText (bankName_, bankField.reduced (10, 0), juce::Justification::centredLeft, 1);

    dock.removeFromLeft (16);

    const auto drawDigits = [&g] (juce::Rectangle<int> field, const juce::String& caption, int digits)
    {
        g.setColour (theme::chassisText);
        theme::drawTrackedText (g, caption, field.removeFromTop (13), theme::labelFont (9.5f), 1.4f);

        theme::drawLcdWell (g, field.toFloat());
        g.setColour (theme::mint);
        g.setFont (theme::lcdFont (15.0f, true));
        g.drawText (juce::String (digits).paddedLeft ('0', 2), field, juce::Justification::centred, false);
    };

    drawDigits (dock.removeFromLeft (54), "BANK", processor_.getMidiBank());
    dock.removeFromLeft (8);
    drawDigits (dock.removeFromLeft (54), "PROG", processor_.getMidiProgram());
}

} // namespace eon
