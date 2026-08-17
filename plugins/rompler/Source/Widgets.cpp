#include "Widgets.h"

namespace eon
{

namespace
{
constexpr float kRotaryStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float kRotaryEnd   = juce::MathConstants<float>::pi * 2.75f;

/** Height reserved under every cell for its name and value. */
constexpr int kCaptionHeight = 26;
} // namespace

// ---------------------------------------------------------------------------
// EonLookAndFeel
// ---------------------------------------------------------------------------

EonLookAndFeel::EonLookAndFeel()
{
    setColour (juce::Slider::thumbColourId, theme::capCream);
}

void EonLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPosProportional, float rotaryStartAngle,
                                       float rotaryEndAngle, juce::Slider& slider)
{
    const auto square = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float diameter = juce::jmin (square.getWidth(), square.getHeight());
    const auto face = juce::Rectangle<float> (diameter, diameter).withCentre (square.getCentre());
    const auto centre = face.getCentre();
    const float radius = diameter * 0.5f;

    const auto cap = slider.findColour (juce::Slider::thumbColourId);
    const bool isHot = cap.getHue() < 0.15f && cap.getSaturation() > 0.4f;

    // Cast shadow first, so the cap sits on the panel rather than floating.
    g.setColour (juce::Colours::black.withAlpha (0.32f));
    g.fillEllipse (face.translated (0.0f, 2.5f).expanded (0.5f));

    juce::ColourGradient body (cap.brighter (isHot ? 0.55f : 0.35f),
                               centre.x - radius * 0.36f, centre.y - radius * 0.44f,
                               isHot ? theme::hotDeep : theme::capCreamShadow,
                               centre.x, centre.y + radius,
                               true);
    body.addColour (0.45, cap);

    g.setGradientFill (body);
    g.fillEllipse (face);

    // Specular highlight, kept inside the rim.
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.45f),
                                             centre.x - radius * 0.34f, centre.y - radius * 0.4f,
                                             juce::Colours::transparentWhite,
                                             centre.x, centre.y + radius * 0.3f,
                                             true));
    g.fillEllipse (face.reduced (diameter * 0.1f));

    const float angle = rotaryStartAngle
                      + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const auto inner = centre.getPointOnCircumference (radius * 0.30f, angle);
    const auto outer = centre.getPointOnCircumference (radius * 0.78f, angle);

    g.setColour (isHot ? theme::onHot : theme::mintDeep);
    g.drawLine ({ inner, outer }, juce::jmax (2.0f, diameter * 0.06f));
}

void EonLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                           const juce::Colour& backgroundColour,
                                           bool shouldDrawButtonAsHighlighted,
                                           bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    const float corner = juce::jmin (5.0f, bounds.getHeight() * 0.3f);

    // A key that travels: the unpressed state carries a hard bottom edge, and
    // pressing collapses it rather than just tinting the fill.
    const float travel = shouldDrawButtonAsDown ? 0.0f : juce::jmin (3.0f, bounds.getHeight() * 0.14f);

    if (travel > 0.0f)
    {
        g.setColour (backgroundColour.darker (0.6f));
        g.fillRoundedRectangle (bounds.withTrimmedTop (travel), corner);
    }

    auto top = bounds.withTrimmedBottom (travel);
    if (shouldDrawButtonAsDown)
        top = top.translated (0.0f, juce::jmin (3.0f, bounds.getHeight() * 0.14f));

    const auto tint = shouldDrawButtonAsHighlighted ? backgroundColour.brighter (0.12f)
                                                    : backgroundColour;

    g.setGradientFill (juce::ColourGradient (tint.brighter (0.45f), top.getCentreX(), top.getY(),
                                             tint, top.getCentreX(), top.getBottom(), false));
    g.fillRoundedRectangle (top, corner);

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (top.reduced (0.5f), corner, 1.0f);
}

void EonLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                     bool, bool shouldDrawButtonAsDown)
{
    auto area = button.getLocalBounds();
    if (shouldDrawButtonAsDown)
        area = area.translated (0, 2);

    g.setColour (button.findColour (juce::TextButton::textColourOffId));

    const float height = juce::jlimit (8.0f, 12.0f, static_cast<float> (area.getHeight()) * 0.42f);
    theme::drawTrackedText (g, button.getButtonText().toUpperCase(), area,
                            theme::labelFont (height), height * 0.16f);
}

// ---------------------------------------------------------------------------
// ParamKnob
// ---------------------------------------------------------------------------

ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& state,
                      const juce::String& parameterID,
                      const juce::String& displayName,
                      bool marksDistortion,
                      int numDecimals)
    : name_ (displayName),
      numDecimals_ (numDecimals),
      attachment_ (state, parameterID, slider_)
{
    if (auto* parameter = state.getParameter (parameterID))
        suffix_ = parameter->getLabel();

    slider_.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider_.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider_.setRotaryParameters (kRotaryStart, kRotaryEnd, true);
    slider_.setColour (juce::Slider::thumbColourId,
                       marksDistortion ? theme::hot : theme::capCream);
    slider_.onValueChange = [this] { repaint(); };

    addAndMakeVisible (slider_);
}

void ParamKnob::paint (juce::Graphics& g)
{
    auto caption = getLocalBounds().removeFromBottom (kCaptionHeight);
    const auto nameArea = caption.removeFromTop (13);

    g.setColour (theme::ink);
    theme::drawTrackedText (g, name_.toUpperCase(), nameArea, theme::labelFont (9.5f), 1.4f);

    g.setColour (theme::inkSoft);
    g.setFont (theme::lcdFont (10.0f));

    const double value = slider_.getValue();
    juce::String text = juce::String (value, numDecimals_);

    if (value > 0.0 && slider_.getMinimum() < 0.0)
        text = "+" + text;

    g.drawText (text + suffix_, caption, juce::Justification::centred, false);
}

void ParamKnob::resized()
{
    slider_.setBounds (getLocalBounds().withTrimmedBottom (kCaptionHeight));
}

// ---------------------------------------------------------------------------
// SegmentedControl
// ---------------------------------------------------------------------------

SegmentedControl::SegmentedControl (juce::AudioProcessorValueTreeState& state,
                                    const juce::String& parameterID,
                                    const juce::String& displayName,
                                    const juce::StringArray& segmentLabels)
    : labels_ (segmentLabels),
      name_ (displayName),
      parameter_ (state.getParameter (parameterID))
{
    jassert (parameter_ != nullptr);

    if (parameter_ != nullptr)
    {
        attachment_ = std::make_unique<juce::ParameterAttachment> (
            *parameter_,
            [this] (float newValue)
            {
                selectedIndex_ = juce::roundToInt (newValue);
                repaint();
            });

        attachment_->sendInitialUpdate();
    }
}

juce::Rectangle<int> SegmentedControl::segmentStrip() const
{
    auto area = getLocalBounds().withTrimmedBottom (kCaptionHeight);
    const int stripHeight = juce::jmin (26, area.getHeight());
    return area.withSizeKeepingCentre (area.getWidth(), stripHeight);
}

void SegmentedControl::paint (juce::Graphics& g)
{
    const auto strip = segmentStrip().toFloat();

    g.setColour (juce::Colours::black.withAlpha (0.14f));
    g.fillRoundedRectangle (strip, 5.0f);

    const int count = juce::jmax (1, labels_.size());
    const float segmentWidth = (strip.getWidth() - 6.0f) / static_cast<float> (count);

    for (int i = 0; i < labels_.size(); ++i)
    {
        const auto cell = juce::Rectangle<float> (strip.getX() + 3.0f + segmentWidth * static_cast<float> (i),
                                                  strip.getY() + 3.0f,
                                                  segmentWidth,
                                                  strip.getHeight() - 6.0f).reduced (1.0f, 0.0f);

        const bool isSelected = (i == selectedIndex_);

        g.setGradientFill (isSelected
            ? juce::ColourGradient (theme::mint, cell.getCentreX(), cell.getY(),
                                    theme::mintDeep, cell.getCentreX(), cell.getBottom(), false)
            : juce::ColourGradient (theme::capCream, cell.getCentreX(), cell.getY(),
                                    theme::capCreamShadow.brighter (0.25f), cell.getCentreX(), cell.getBottom(), false));
        g.fillRoundedRectangle (cell, 3.0f);

        g.setColour (isSelected ? theme::onMint : theme::inkSoft);
        theme::drawTrackedText (g, labels_[i].toUpperCase(), cell.toNearestInt(),
                                theme::labelFont (8.5f), 0.6f);
    }

    g.setColour (theme::ink);
    theme::drawTrackedText (g, name_.toUpperCase(),
                            getLocalBounds().removeFromBottom (kCaptionHeight).removeFromTop (13),
                            theme::labelFont (9.5f), 1.4f);
}

void SegmentedControl::mouseDown (const juce::MouseEvent& event)
{
    const auto strip = segmentStrip();

    if (! strip.contains (event.getPosition()) || labels_.isEmpty() || attachment_ == nullptr)
        return;

    const float relative = static_cast<float> (event.position.x - static_cast<float> (strip.getX()))
                         / static_cast<float> (strip.getWidth());
    const int index = juce::jlimit (0, labels_.size() - 1,
                                    static_cast<int> (relative * static_cast<float> (labels_.size())));

    attachment_->setValueAsCompleteGesture (static_cast<float> (index));
}

// ---------------------------------------------------------------------------
// Stepper
// ---------------------------------------------------------------------------

Stepper::Stepper (juce::AudioProcessorValueTreeState& state,
                  const juce::String& parameterID,
                  const juce::String& displayName)
    : name_ (displayName),
      parameter_ (state.getParameter (parameterID))
{
    jassert (parameter_ != nullptr);

    if (parameter_ != nullptr)
    {
        attachment_ = std::make_unique<juce::ParameterAttachment> (
            *parameter_,
            [this] (float newValue)
            {
                value_ = juce::roundToInt (newValue);
                repaint();
            });

        attachment_->sendInitialUpdate();
    }

    for (auto* button : { &up_, &down_ })
    {
        button->setColour (juce::TextButton::buttonColourId, theme::capCream);
        button->setColour (juce::TextButton::textColourOffId, theme::ink);
        addAndMakeVisible (*button);
    }

    up_.onClick   = [this] { nudge (1); };
    down_.onClick = [this] { nudge (-1); };
}

void Stepper::nudge (int direction)
{
    if (parameter_ == nullptr || attachment_ == nullptr)
        return;

    const auto range = parameter_->getNormalisableRange();
    const float target = juce::jlimit (range.start, range.end,
                                       static_cast<float> (value_ + direction));

    attachment_->setValueAsCompleteGesture (target);
}

juce::Rectangle<int> Stepper::readoutArea() const
{
    auto area = getLocalBounds().withTrimmedBottom (kCaptionHeight);
    area = area.withSizeKeepingCentre (juce::jmin (72, area.getWidth()), juce::jmin (26, area.getHeight()));
    return area.withTrimmedRight (20);
}

void Stepper::paint (juce::Graphics& g)
{
    const auto well = readoutArea().toFloat();
    theme::drawLcdWell (g, well);

    g.setColour (theme::mint);
    g.setFont (theme::lcdFont (15.0f, true));
    g.drawText (juce::String (value_).paddedLeft ('0', 2), well.toNearestInt(),
                juce::Justification::centred, false);

    g.setColour (theme::ink);
    theme::drawTrackedText (g, name_.toUpperCase(),
                            getLocalBounds().removeFromBottom (kCaptionHeight).removeFromTop (13),
                            theme::labelFont (9.5f), 1.4f);
}

void Stepper::resized()
{
    const auto well = readoutArea();
    auto arrows = juce::Rectangle<int> (well.getRight() + 3, well.getY(), 17, well.getHeight());

    up_.setBounds (arrows.removeFromTop (arrows.getHeight() / 2).reduced (0, 1));
    down_.setBounds (arrows.reduced (0, 1));
}

// ---------------------------------------------------------------------------
// PeakMeter
// ---------------------------------------------------------------------------

PeakMeter::PeakMeter (std::function<float()> levelSource)
    : levelSource_ (std::move (levelSource))
{
    startTimerHz (30);
}

void PeakMeter::timerCallback()
{
    const float level = levelSource_ != nullptr ? levelSource_() : 0.0f;

    int target = 0;
    if (level > 1.0e-5f)
    {
        const float decibels = juce::Decibels::gainToDecibels (level, -48.0f);
        target = juce::jlimit (0, numLeds,
                               juce::roundToInt (juce::jmap (decibels, -48.0f, 0.0f,
                                                             0.0f, static_cast<float> (numLeds))));
    }

    // Rises instantly, falls a rung at a time: an unsmoothed ladder reads as
    // noise rather than as level.
    const int updated = target > litLeds_ ? target : juce::jmax (target, litLeds_ - 1);

    if (updated != litLeds_)
    {
        litLeds_ = updated;
        repaint();
    }
}

void PeakMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().withTrimmedBottom (kCaptionHeight);

    const int ledWidth = 6;
    const int gap = 3;
    const int totalWidth = numLeds * ledWidth + (numLeds - 1) * gap;

    auto row = juce::Rectangle<int> (totalWidth, juce::jmin (14, area.getHeight()))
                   .withCentre (area.getCentre());

    for (int i = 0; i < numLeds; ++i)
    {
        const auto led = juce::Rectangle<int> (row.getX() + i * (ledWidth + gap), row.getY(),
                                               ledWidth, row.getHeight()).toFloat();

        const bool lit = i < litLeds_;
        const auto colour = i >= numLeds - 2 ? theme::ledRed
                          : i >= numLeds - 4 ? theme::ledHot
                                             : theme::ledMint;

        if (lit)
        {
            g.setColour (colour.withAlpha (0.35f));
            g.fillRoundedRectangle (led.expanded (1.5f), 2.0f);
        }

        g.setColour (lit ? colour : theme::ledOff);
        g.fillRoundedRectangle (led, 1.5f);
    }

    g.setColour (theme::ink);
    theme::drawTrackedText (g, "PEAK",
                            getLocalBounds().removeFromBottom (kCaptionHeight).removeFromTop (13),
                            theme::labelFont (9.5f), 1.4f);
}

} // namespace eon
