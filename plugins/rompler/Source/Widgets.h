#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "Theme.h"

namespace eon
{

/**
    Panel styling for the stock JUCE controls.

    A knob's cap colour travels on the slider itself as thumbColourId, so the
    four distortion-shaping controls can be marked orange without the
    LookAndFeel needing to know which parameters they are.
*/
class EonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    EonLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EonLookAndFeel)
};

/**
    One labelled control cell on the panel: a rotary cap, its silkscreened name
    and its value.

    The cell owns the layout rather than relying on Slider's own text box,
    because the panel reads knob → name → value and the built-in box can only
    sit above or below the whole thing.
*/
class ParamKnob final : public juce::Component
{
public:
    ParamKnob (juce::AudioProcessorValueTreeState& state,
               const juce::String& parameterID,
               const juce::String& displayName,
               bool marksDistortion,
               int numDecimals = 0);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider_;
    juce::String name_;
    juce::String suffix_;
    int numDecimals_;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamKnob)
};

/**
    The panel's segmented selector, for choice parameters.

    Uses ParameterAttachment rather than ComboBoxAttachment: every option is on
    the surface at once, which is how a hardware selector behaves and how the
    approved design draws it.
*/
class SegmentedControl final : public juce::Component
{
public:
    SegmentedControl (juce::AudioProcessorValueTreeState& state,
                      const juce::String& parameterID,
                      const juce::String& displayName,
                      const juce::StringArray& segmentLabels);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::StringArray labels_;
    juce::String name_;
    int selectedIndex_ = 0;
    juce::RangedAudioParameter* parameter_ = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment_;

    [[nodiscard]] juce::Rectangle<int> segmentStrip() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedControl)
};

/** Lit digit readout with up/down arrows, for the integer polyphony limit. */
class Stepper final : public juce::Component
{
public:
    Stepper (juce::AudioProcessorValueTreeState& state,
             const juce::String& parameterID,
             const juce::String& displayName);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String name_;
    int value_ = 0;
    juce::RangedAudioParameter* parameter_ = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment_;
    juce::TextButton up_ { "+" };
    juce::TextButton down_ { "-" };

    [[nodiscard]] juce::Rectangle<int> readoutArea() const;
    void nudge (int direction);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Stepper)
};

/** Output peak ladder. Polls the processor rather than being pushed to. */
class PeakMeter final : public juce::Component,
                        private juce::Timer
{
public:
    explicit PeakMeter (std::function<float()> levelSource);
    ~PeakMeter() override = default;

    void paint (juce::Graphics&) override;

private:
    static constexpr int numLeds = 9;

    void timerCallback() override;

    std::function<float()> levelSource_;
    int litLeds_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PeakMeter)
};

} // namespace eon
