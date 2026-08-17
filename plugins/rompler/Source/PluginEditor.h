#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "Theme.h"
#include "Widgets.h"

namespace eon
{

/**
    The EON-50 panel.

    A dark chassis carrying a cream control surface, split into two
    mint-outlined groups — VOICE (what each note does) and BUS (what the summed
    output does) — over a dock holding the loaded bank.

    Fixed size on purpose: the layout is a hardware panel with a fixed number of
    cells, so there is nothing for a resize to reflow.
*/
class RomplerEditor final : public juce::AudioProcessorEditor
{
public:
    explicit RomplerEditor (RomplerProcessor&);
    ~RomplerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** A group box: mint outline, cream fill, tab label over the top edge. */
    struct Group
    {
        juce::String title;
        juce::Rectangle<int> bounds;
        std::vector<juce::Component*> cells;
    };

    void openSoundFontChooser();
    void refreshBankReadout();
    void paintGroup (juce::Graphics&, const Group&) const;
    static void layOutCells (const Group&);

    RomplerProcessor& processor_;
    EonLookAndFeel lookAndFeel_;

    ParamKnob drive_, velToDrive_, filterOffset_;
    SegmentedControl curve_, filterRouting_;
    Stepper polyphony_;

    ParamKnob tapeDrive_, fold_, outTrim_, mix_;
    SegmentedControl oversampling_;
    PeakMeter meter_;

    juce::TextButton loadButton_ { "Load" };
    juce::String bankName_ { "NO FILE LOADED" };

    Group voiceGroup_, busGroup_;
    juce::Rectangle<int> dockBounds_, nameplateBounds_;

    std::unique_ptr<juce::FileChooser> chooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomplerEditor)
};

} // namespace eon
