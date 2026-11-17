#include "PluginEditor.h"

#include <cmath>

namespace aod
{

namespace
{

class DeckLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return makeFont (9.2f, true);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool isHighlighted, bool isDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const auto fill = isDown        ? theme::jadeDeep.withAlpha (0.45f)
                        : isHighlighted ? theme::jadeWash.brighter (0.18f)
                                        : theme::jadeWash;
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (isHighlighted ? theme::jade : theme::jadeDeep);
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return makeFont (9.5f, true);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f,
                                                    static_cast<float> (width),
                                                    static_cast<float> (height)).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        const float cx = static_cast<float> (width) - 13.0f;
        const float cy = static_cast<float> (height) * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath (cx - 3.5f, cy - 1.5f);
        arrow.lineTo (cx, cy + 2.5f);
        arrow.lineTo (cx + 3.5f, cy - 1.5f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override
    {
        if (thumbSize <= 0)
            return;

        const auto thumb = isScrollbarVertical
            ? juce::Rectangle<float> (static_cast<float> (x) + 2.0f,
                                      static_cast<float> (thumbStartPosition),
                                      juce::jmax (3.0f, static_cast<float> (width) - 4.0f),
                                      static_cast<float> (thumbSize))
            : juce::Rectangle<float> (static_cast<float> (thumbStartPosition),
                                      static_cast<float> (y) + 2.0f,
                                      static_cast<float> (thumbSize),
                                      juce::jmax (3.0f, static_cast<float> (height) - 4.0f));

        const auto colour = (isMouseOver || isMouseDown) ? theme::jade : theme::jadeDeep.withAlpha (0.8f);
        g.setColour (colour);
        g.fillRoundedRectangle (thumb, 2.5f);
    }
};

void drawMakerSeal (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (theme::sealRed);
    g.fillRoundedRectangle (r, 2.5f);

    // Inner hairline chop border + minimal architectural lattice mark.
    const auto inner = r.reduced (2.5f);
    g.setColour (theme::inkLight.withAlpha (0.85f));
    g.drawRect (inner, 1.0f);

    const float cx = inner.getCentreX();
    const float cy = inner.getCentreY();
    g.drawLine (inner.getX() + 2.0f, cy, inner.getRight() - 2.0f, cy, 1.1f);
    g.drawLine (cx, inner.getY() + 2.0f, cx, inner.getBottom() - 2.0f, 1.1f);
    g.drawLine (inner.getX() + 2.0f, inner.getY() + 2.5f, inner.getX() + 5.0f, inner.getY() + 2.5f, 1.0f);
    g.drawLine (inner.getRight() - 5.0f, inner.getBottom() - 2.5f, inner.getRight() - 2.0f, inner.getBottom() - 2.5f, 1.0f);
}

juce::String formatKnobValue (const juce::RangedAudioParameter& param, double rawValue, bool bipolar)
{
    const double cleanVal = (std::abs (rawValue) < 0.005) ? 0.0 : rawValue;
    const auto unit = param.getLabel().trim();
    const juce::String sign = (bipolar && cleanVal > 0.005) ? "+" : "";

    if (unit == "cents")
        return sign + juce::String (juce::roundToInt (cleanVal)) + " ct";
    if (unit == "dB")
        return sign + juce::String (cleanVal, 1) + " dB";
    if (unit == "%")
        return sign + juce::String (cleanVal, 1) + "%";
    if (unit == "Hz")
        return juce::String (cleanVal, 2) + " Hz";

    return param.getText (param.convertTo0to1 (static_cast<float> (cleanVal)), 16);
}

} // namespace

// ============================================================================
// SectionBox
// ============================================================================

SectionBox::SectionBox (const juce::String& title,
                        const juce::String& subtitle,
                        bool splitMid)
    : splitMid_ (splitMid)
{
    addAndMakeVisible (title_);
    title_.setText (title, juce::dontSendNotification);
    title_.setJustificationType (juce::Justification::centredLeft);
    title_.setColour (juce::Label::textColourId, theme::ink);
    title_.setFont (makeFont (10.0f, true));

    if (subtitle.isNotEmpty())
    {
        addAndMakeVisible (subtitle_);
        subtitle_.setText (subtitle, juce::dontSendNotification);
        subtitle_.setJustificationType (juce::Justification::centredRight);
        subtitle_.setColour (juce::Label::textColourId, theme::inkSoft);
        subtitle_.setFont (makeFont (8.5f, false));
    }
}

SectionBox::~SectionBox() = default;

void SectionBox::resized()
{
    const int boxW = getWidth();
    title_.setBounds (23, 5, 90, 17);
    if (subtitle_.isVisible())
        subtitle_.setBounds (110, 6, juce::jmax (0, boxW - 122), 15);
}

void SectionBox::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    // Subtle top bevel highlight + crisp mineral border.
    g.setColour (theme::inkLight.withAlpha (0.40f));
    g.drawHorizontalLine (1, 6.0f, bounds.getWidth() - 6.0f);
    g.setColour (theme::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    // Celadon index square + integrated header hairline.
    g.setColour (theme::jadeDeep);
    g.fillRoundedRectangle (12.0f, 11.0f, 5.0f, 5.0f, 1.0f);

    g.setColour (theme::panelRule);
    g.drawLine (12.0f, 26.5f, bounds.getWidth() - 12.0f, 26.5f, 1.0f);

    if (splitMid_)
    {
        const float midX = std::floor (bounds.getWidth() * 0.5f) + 0.5f;
        g.setColour (theme::panelRule.withAlpha (0.75f));
        g.drawLine (midX, 36.0f, midX, bounds.getHeight() - 12.0f, 1.0f);
    }
}

// ============================================================================
// Knob
// ============================================================================

Knob::Knob (juce::RangedAudioParameter& param, bool hot, bool bipolar)
    : param_ (param), hot_ (hot), bipolar_ (bipolar)
{
    slider_.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider_.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    attachment_ = std::make_unique<juce::SliderParameterAttachment> (param, slider_);

    param_.addListener (this);

    addAndMakeVisible (name_);
    name_.setText (param.getName (32).toUpperCase(), juce::dontSendNotification);
    name_.setJustificationType (juce::Justification::centred);
    name_.setColour (juce::Label::textColourId, theme::ink);
    name_.setFont (makeFont (9.0f, true));

    addAndMakeVisible (value_);
    value_.setJustificationType (juce::Justification::centred);
    value_.setColour (juce::Label::textColourId, theme::inkSoft);
    value_.setFont (makeFont (9.0f, false));
    value_.setText (formatKnobValue (param_, slider_.getValue(), bipolar_), juce::dontSendNotification);

    setSize (90, 96);
}

Knob::~Knob()
{
    param_.removeListener (this);
}

void Knob::parameterValueChanged (int, float)
{
    triggerAsyncUpdate();
}

void Knob::handleAsyncUpdate()
{
    value_.setText (formatKnobValue (param_, slider_.getValue(), bipolar_), juce::dontSendNotification);
    repaint();
}

void Knob::paint (juce::Graphics& g)
{
    const float w = static_cast<float> (getWidth());
    const float h = static_cast<float> (getHeight());
    const float dialSpaceH = juce::jmax (36.0f, h - 34.0f);
    const float knobSize = juce::jmin (w - 18.0f, dialSpaceH - 10.0f, 44.0f);

    const float cy = 18.0f + dialSpaceH * 0.5f;
    const auto circle = juce::Rectangle<float> (w * 0.5f - knobSize * 0.5f,
                                                cy - knobSize * 0.5f,
                                                knobSize,
                                                knobSize);
    const auto centre = circle.getCentre();

    // 270-degree arc measured clockwise from 12 o'clock.
    constexpr float kMaxSweep = juce::MathConstants<float>::pi * 0.75f;
    const float unit = static_cast<float> (param_.convertTo0to1 (static_cast<float> (slider_.getValue())));
    const float angleRad = (unit * 2.0f - 1.0f) * kMaxSweep;
    const float arcR = knobSize * 0.5f + 4.5f;

    // Track arc.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, -kMaxSweep, kMaxSweep, true);
    g.setColour (theme::panelRule);
    g.strokePath (track, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolar center detent tick at 12 o'clock.
    if (bipolar_)
    {
        g.setColour (theme::inkSoft.withAlpha (0.7f));
        g.drawLine (centre.x, centre.y - arcR - 2.5f, centre.x, centre.y - arcR + 1.5f, 1.2f);
    }

    // Active value arc.
    const auto accent = hot_ ? theme::cinnabarDeep : theme::jadeDeep;
    const float arcStart = bipolar_ ? juce::jmin (0.0f, angleRad) : -kMaxSweep;
    const float arcEnd   = bipolar_ ? juce::jmax (0.0f, angleRad) : angleRad;
    if (arcEnd - arcStart > 0.02f)
    {
        juce::Path active;
        active.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, arcStart, arcEnd, true);
        g.setColour (accent);
        g.strokePath (active, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Soft shadow under porcelain cap.
    g.setColour (juce::Colour (0x24000000));
    g.fillEllipse (circle.translated (0.0f, 1.5f));

    // Matte ceramic / warm lacquer cap.
    const auto capTop = hot_ ? juce::Colour (0xfff6ebe2) : juce::Colour (0xfff8f4ea);
    const auto capBot = hot_ ? juce::Colour (0xffdfd0c1) : theme::knobShadow;
    juce::ColourGradient grad (capTop, circle.getTopLeft(), capBot, circle.getBottomLeft(), false);
    g.setGradientFill (grad);
    g.fillEllipse (circle);

    g.setColour (hot_ ? theme::cinnabarDeep.withAlpha (0.38f) : juce::Colour (0x36000000));
    g.drawEllipse (circle, 1.0f);

    // Machined indicator line (0 = 12 o'clock).
    const float rOuter = knobSize * 0.5f - 3.5f;
    const float rInner = knobSize * 0.14f;
    const float dx = std::sin (angleRad);
    const float dy = -std::cos (angleRad);
    g.setColour (accent);
    g.drawLine (centre.x + rInner * dx, centre.y + rInner * dy,
                centre.x + rOuter * dx, centre.y + rOuter * dy, 2.0f);

    value_.setText (formatKnobValue (param_, slider_.getValue(), bipolar_), juce::dontSendNotification);
}

void Knob::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    name_.setBounds (0, 2, w, 14);
    value_.setBounds (0, h - 14, w, 12);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    lastDragY_ = e.position.y;
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    const auto range = param_.getNormalisableRange();
    const float pixelsPerFullRange = e.mods.isShiftDown() ? 600.0f : 200.0f;
    const float delta = (lastDragY_ - e.position.y) * (range.end - range.start) / pixelsPerFullRange;
    lastDragY_ = e.position.y;
    slider_.setValue (slider_.getValue() + delta, juce::sendNotificationSync);
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    slider_.setValue (param_.convertFrom0to1 (param_.getDefaultValue()), juce::sendNotificationSync);
}

void Knob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const auto range = param_.getNormalisableRange();
    const float step = (range.end - range.start) / 100.0f;
    slider_.setValue (slider_.getValue() + wheel.deltaY * step, juce::sendNotificationSync);
}

// ============================================================================
// Switch (segmented selector)
// ============================================================================

Switch::Switch (juce::AudioParameterChoice& param, const juce::String& label, bool leds)
    : param_ (param), leds_ (leds)
{
    addAndMakeVisible (label_);
    label_.setText (label.isEmpty() ? param.getName (32).toUpperCase() : label, juce::dontSendNotification);
    label_.setJustificationType (juce::Justification::centred);
    label_.setColour (juce::Label::textColourId, theme::ink);
    label_.setFont (makeFont (9.0f, true));

    for (const auto& choice : param.getAllValueStrings())
        box_.addItem (choice, box_.getNumItems() + 1);
    attachment_ = std::make_unique<juce::ComboBoxParameterAttachment> (param, box_);

    setSize (114, 96);
}

Switch::~Switch() = default;

void Switch::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    label_.setBounds (0, 2, w, 14);

    const int pillW = juce::jlimit (64, w - 8, 108);
    const int pillH = 24;
    const int dialCentreY = 18 + (h - 34) / 2;
    const int pillY = dialCentreY - pillH / 2 - (leds_ ? 3 : 0);
    pill_ = juce::Rectangle<int> ((w - pillW) / 2, pillY, pillW, pillH);
}

void Switch::advanceChoice()
{
    const int n = box_.getNumItems();
    if (n <= 1)
        return;
    const int cur = box_.getSelectedId();
    box_.setSelectedId (cur >= n ? 1 : cur + 1, juce::sendNotificationSync);
    repaint();
}

void Switch::paint (juce::Graphics& g)
{
    const auto well = pill_.toFloat();
    g.setColour (theme::displayBg);
    g.fillRoundedRectangle (well, 4.0f);

    const int n = box_.getNumItems();
    const int selectedIdx = box_.getSelectedItemIndex();

    if (n > 0)
    {
        const float segW = well.getWidth() / static_cast<float> (n);
        g.setFont (makeFont (n >= 4 ? 8.2f : 8.0f, true));

        for (int i = 0; i < n; ++i)
        {
            const auto seg = juce::Rectangle<float> (well.getX() + static_cast<float> (i) * segW,
                                                     well.getY(), segW, well.getHeight());
            const bool active = (i == selectedIdx);
            if (active)
            {
                g.setColour (theme::jadeWash);
                g.fillRoundedRectangle (seg.reduced (1.5f), 2.5f);
                g.setColour (theme::jade);
                g.fillRect (seg.getX() + 4.0f, seg.getBottom() - 2.5f, seg.getWidth() - 8.0f, 1.5f);
            }
            else if (i > 0)
            {
                g.setColour (theme::displayEdge);
                g.drawVerticalLine (juce::roundToInt (seg.getX()), well.getY() + 4.0f, well.getBottom() - 4.0f);
            }

            juce::String textLabel = box_.getItemText (i).toUpperCase();
            if (textLabel == "TRANSFORMER")
                textLabel = "XFMR";

            g.setColour (active ? theme::inkLight : theme::inkMuted);
            g.drawText (textLabel, seg, juce::Justification::centred, false);
        }
    }

    g.setColour (theme::displayEdge);
    g.drawRoundedRectangle (well, 4.0f, 1.0f);

    // Restrained step indicators for oversampling: 4 pips matching the 4 choices.
    if (leds_ && n > 0)
    {
        const float pipW = 10.0f;
        const float pipH = 3.0f;
        const float gap = 5.0f;
        const float totalW = static_cast<float> (n) * pipW + static_cast<float> (n - 1) * gap;
        const float startX = well.getCentreX() - totalW * 0.5f;
        const float pipY = well.getBottom() + 6.0f;

        for (int i = 0; i < n; ++i)
        {
            const bool lit = (i <= selectedIdx);
            const auto pipColour = !lit ? theme::panelRule
                                 : (i == n - 1) ? theme::cinnabar
                                                : theme::jadeDeep;
            g.setColour (pipColour);
            g.fillRoundedRectangle (startX + static_cast<float> (i) * (pipW + gap),
                                    pipY, pipW, pipH, 1.5f);
        }
    }
}

void Switch::mouseDown (const juce::MouseEvent& e)
{
    const int n = box_.getNumItems();
    if (n <= 0)
        return;

    if (pill_.contains (e.getPosition()))
    {
        const float relX = static_cast<float> (e.getPosition().x - pill_.getX());
        const int idx = juce::jlimit (0, n - 1, static_cast<int> (relX * static_cast<float> (n) / static_cast<float> (pill_.getWidth())));
        box_.setSelectedId (idx + 1, juce::sendNotificationSync);
        repaint();
    }
    else
    {
        advanceChoice();
    }
}

void Switch::mouseUp (const juce::MouseEvent&)
{
}

void Switch::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const int n = box_.getNumItems();
    if (n <= 1)
        return;
    const int cur = box_.getSelectedId();
    if (wheel.deltaY > 0.0f)
        box_.setSelectedId (cur <= 1 ? n : cur - 1, juce::sendNotificationSync);
    else if (wheel.deltaY < 0.0f)
        box_.setSelectedId (cur >= n ? 1 : cur + 1, juce::sendNotificationSync);
    repaint();
}

// ============================================================================
// Toggle (2-way)
// ============================================================================

Toggle::Toggle (juce::AudioParameterChoice& param, const juce::String& label)
    : param_ (param)
{
    addAndMakeVisible (label_);
    label_.setText (label.isEmpty() ? param.getName (32).toUpperCase() : label, juce::dontSendNotification);
    label_.setJustificationType (juce::Justification::centred);
    label_.setColour (juce::Label::textColourId, theme::ink);
    label_.setFont (makeFont (9.0f, true));

    for (const auto& choice : param.getAllValueStrings())
        box_.addItem (choice, box_.getNumItems() + 1);
    attachment_ = std::make_unique<juce::ComboBoxParameterAttachment> (param, box_);

    setSize (114, 96);
}

Toggle::~Toggle() = default;

void Toggle::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    label_.setBounds (0, 2, w, 14);

    const int pillW = juce::jlimit (64, w - 12, 88);
    const int pillH = 24;
    const int dialCentreY = 18 + (h - 34) / 2;
    pill_ = juce::Rectangle<int> ((w - pillW) / 2, dialCentreY - pillH / 2, pillW, pillH);
}

void Toggle::paint (juce::Graphics& g)
{
    const auto well = pill_.toFloat();
    g.setColour (theme::displayBg);
    g.fillRoundedRectangle (well, 4.0f);

    const int selectedIdx = box_.getSelectedItemIndex();
    const float halfW = well.getWidth() * 0.5f;
    g.setFont (makeFont (8.5f, true));

    for (int i = 0; i < 2; ++i)
    {
        const auto seg = juce::Rectangle<float> (well.getX() + static_cast<float> (i) * halfW,
                                                 well.getY(), halfW, well.getHeight());
        const bool active = (i == selectedIdx);
        if (active)
        {
            g.setColour (theme::jadeWash);
            g.fillRoundedRectangle (seg.reduced (1.5f), 2.5f);
            g.setColour (theme::jade);
            g.fillRect (seg.getX() + 6.0f, seg.getBottom() - 2.5f, seg.getWidth() - 12.0f, 1.5f);
        }

        const juce::String name = (i < box_.getNumItems()) ? box_.getItemText (i).toUpperCase()
                                                           : (i == 0 ? "PRE" : "POST");
        g.setColour (active ? theme::inkLight : theme::inkMuted);
        g.drawText (name, seg, juce::Justification::centred, false);
    }

    g.setColour (theme::displayEdge);
    g.drawRoundedRectangle (well, 4.0f, 1.0f);
}

void Toggle::mouseDown (const juce::MouseEvent& e)
{
    if (pill_.contains (e.getPosition()))
    {
        const int targetId = (e.getPosition().x < pill_.getCentreX()) ? 1 : 2;
        if (box_.getSelectedId() != targetId)
            box_.setSelectedId (targetId, juce::sendNotificationSync);
        else
            box_.setSelectedId ((targetId == 1) ? 2 : 1, juce::sendNotificationSync);
    }
    else
    {
        box_.setSelectedId ((box_.getSelectedId() == 1) ? 2 : 1, juce::sendNotificationSync);
    }
    repaint();
}

// ============================================================================
// Stepper (polyphony)
// ============================================================================

Stepper::Stepper (juce::RangedAudioParameter& param, const juce::String& label)
    : param_ (param)
{
    addAndMakeVisible (label_);
    label_.setText (label.isEmpty() ? param.getName (32).toUpperCase() : label, juce::dontSendNotification);
    label_.setJustificationType (juce::Justification::centred);
    label_.setColour (juce::Label::textColourId, theme::ink);
    label_.setFont (makeFont (9.0f, true));

    slider_.setRange (param.getNormalisableRange().start, param.getNormalisableRange().end, 1.0);
    slider_.setValue (param.convertFrom0to1 (param.getValue()), juce::dontSendNotification);
    attachment_ = std::make_unique<juce::SliderParameterAttachment> (param, slider_);

    param_.addListener (this);

    setSize (114, 96);
}

Stepper::~Stepper()
{
    param_.removeListener (this);
}

void Stepper::parameterValueChanged (int, float)
{
    triggerAsyncUpdate();
}

void Stepper::handleAsyncUpdate()
{
    repaint();
}

void Stepper::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    label_.setBounds (0, 2, w, 14);

    const int boxW = juce::jlimit (64, w - 16, 82);
    const int boxH = 24;
    const int dialCentreY = 18 + (h - 34) / 2;
    readoutRect_ = juce::Rectangle<int> ((w - boxW) / 2, dialCentreY - boxH / 2, boxW, boxH);
}

void Stepper::paint (juce::Graphics& g)
{
    const auto r = readoutRect_.toFloat();
    const int value = juce::roundToInt (slider_.getValue());

    g.setColour (theme::displayBg);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (theme::displayEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    // Subtle - and + flank glyphs.
    g.setColour (theme::inkMuted);
    g.setFont (makeFont (11.0f, true));
    g.drawText ("-", r.withWidth (18.0f), juce::Justification::centred, false);
    g.drawText ("+", r.withLeft (r.getRight() - 18.0f), juce::Justification::centred, false);

    g.setColour (theme::jade);
    g.setFont (makeFont (12.0f, true));
    g.drawText (juce::String (value).paddedLeft ('0', 2),
                r.reduced (16.0f, 0.0f), juce::Justification::centred, false);
}

void Stepper::mouseDown (const juce::MouseEvent& e)
{
    lastDragY_ = e.position.y;
    dragged_ = false;
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void Stepper::mouseUp (const juce::MouseEvent& e)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    if (!dragged_ && readoutRect_.contains (e.getPosition()))
    {
        if (e.getPosition().x < readoutRect_.getX() + 20)
            slider_.setValue (slider_.getValue() - 1.0, juce::sendNotificationSync);
        else if (e.getPosition().x > readoutRect_.getRight() - 20)
            slider_.setValue (slider_.getValue() + 1.0, juce::sendNotificationSync);
    }
}

void Stepper::mouseDrag (const juce::MouseEvent& e)
{
    if (std::abs (lastDragY_ - e.position.y) > 2.0f)
        dragged_ = true;

    const auto range = param_.getNormalisableRange();
    const float pixelsPerFullRange = 200.0f;
    const float delta = (lastDragY_ - e.position.y) * (range.end - range.start) / pixelsPerFullRange;
    lastDragY_ = e.position.y;
    slider_.setValue (slider_.getValue() + delta, juce::sendNotificationSync);
}

void Stepper::mouseDoubleClick (const juce::MouseEvent&)
{
    slider_.setValue (param_.convertFrom0to1 (param_.getDefaultValue()), juce::sendNotificationSync);
}

void Stepper::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const double step = (wheel.deltaY > 0.0f) ? 1.0 : (wheel.deltaY < 0.0f ? -1.0 : 0.0);
    slider_.setValue (slider_.getValue() + step, juce::sendNotificationSync);
}

// ============================================================================
// PeakMeter
// ============================================================================

PeakMeter::PeakMeter()
{
    addAndMakeVisible (label_);
    label_.setText ("OUTPUT", juce::dontSendNotification);
    label_.setJustificationType (juce::Justification::centred);
    label_.setColour (juce::Label::textColourId, theme::ink);
    label_.setFont (makeFont (9.0f, true));

    setSize (114, 96);
}

PeakMeter::~PeakMeter() = default;

void PeakMeter::setLevel (float level)
{
    level_ = juce::jlimit (0.0f, 1.0f, level);
    repaint();
}

void PeakMeter::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    label_.setBounds (0, 2, w, 14);

    const int barW = juce::jlimit (64, w - 16, 92);
    const int barH = 18;
    const int dialCentreY = 18 + (h - 34) / 2;
    barRect_ = juce::Rectangle<int> ((w - barW) / 2, dialCentreY - barH / 2, barW, barH);
}

void PeakMeter::paint (juce::Graphics& g)
{
    const auto well = barRect_.toFloat();
    g.setColour (theme::displayBg);
    g.fillRoundedRectangle (well, 4.0f);
    g.setColour (theme::displayEdge);
    g.drawRoundedRectangle (well, 4.0f, 1.0f);

    const auto inner = well.reduced (4.0f, 4.0f);
    const float gap = 2.5f;
    const float ledW = (inner.getWidth() - gap * static_cast<float> (numSegments - 1)) / static_cast<float> (numSegments);
    const int lit = juce::roundToInt (level_ * static_cast<float> (numSegments));

    for (int i = 0; i < numSegments; ++i)
    {
        const auto seg = juce::Rectangle<float> (inner.getX() + static_cast<float> (i) * (ledW + gap),
                                                 inner.getY(), ledW, inner.getHeight());
        const bool on = i < lit;
        const auto colour = (i >= numSegments - 1) ? theme::sealRed
                          : (i >= numSegments - 3) ? theme::cinnabar
                                                   : theme::jade;
        g.setColour (on ? colour : theme::ledOff);
        g.fillRoundedRectangle (seg, 1.5f);
    }
}

// ============================================================================
// Keyboard
// ============================================================================

Keyboard::Keyboard()
{
    setSize (800, 100);
}

Keyboard::~Keyboard() = default;

void Keyboard::setKeyRange (int lowNote, int numKeys)
{
    keys_.clear();
    lit_.clear();
    numWhite_ = 0;

    // Standard chromatic layout within an octave (C, C#, D, D#, E, F, F#, G, G#, A, A#, B).
    static constexpr bool isBlack[12] = { false, true, false, true, false, false,
                                          true, false, true, false, true, false };
    for (int i = 0; i < numKeys; ++i)
    {
        const int note = lowNote + i;
        const int pc = ((note % 12) + 12) % 12;
        if (isBlack[pc])
            keys_.push_back ({ note, true, juce::jmax (0, numWhite_ - 1) });
        else
        {
            keys_.push_back ({ note, false, numWhite_ });
            ++numWhite_;
        }
    }
    repaint();
}

juce::Rectangle<int> Keyboard::boundsFor (const PianoKey& k) const
{
    const auto b = getLocalBounds().withTrimmedTop (3);
    const float whiteW = (numWhite_ > 0) ? static_cast<float> (b.getWidth()) / static_cast<float> (numWhite_) : 0.0f;

    if (!k.black)
    {
        const int x0 = b.getX() + static_cast<int> (std::floor (static_cast<float> (k.whiteIndex) * whiteW));
        const int x1 = b.getX() + static_cast<int> (std::floor (static_cast<float> (k.whiteIndex + 1) * whiteW));
        return { x0, b.getY(), juce::jmax (1, x1 - x0), b.getHeight() };
    }

    const int blackW = juce::jmax (6, static_cast<int> (std::lround (whiteW * 0.60f)));
    const int seamX = b.getX() + static_cast<int> (std::lround (static_cast<float> (k.whiteIndex + 1) * whiteW));
    const int blackH = static_cast<int> (std::lround (static_cast<float> (b.getHeight()) * 0.61f));
    return { seamX - blackW / 2, b.getY(), blackW, blackH };
}

int Keyboard::findNote (juce::Point<float> p) const
{
    for (const auto& k : keys_)
        if (k.black && boundsFor (k).toFloat().contains (p))
            return k.note;
    for (const auto& k : keys_)
        if (!k.black && boundsFor (k).toFloat().contains (p))
            return k.note;
    return -1;
}

void Keyboard::setNoteOn (int note, bool on)
{
    lit_.set (note, on);
    repaint();
}

void Keyboard::paint (juce::Graphics& g)
{
    g.fillAll (theme::body2);

    // Traditional red-felt keybed rail along the top edge.
    g.setColour (theme::keyFelt);
    g.fillRect (0, 0, getWidth(), 3);

    for (const auto& k : keys_)
        if (!k.black)
        {
            const auto r = boundsFor (k).toFloat();
            const bool lit = lit_[k.note];
            const juce::Colour top = lit ? theme::jade : juce::Colour (0xffece6d5);
            const juce::Colour bot = lit ? theme::jadeDeep : juce::Colour (0xffd7ceb7);
            g.setGradientFill (juce::ColourGradient (top, r.getTopLeft(), bot, r.getBottomLeft(), false));
            g.fillRoundedRectangle (r, 1.5f);
            g.setColour (juce::Colour (0xff8a8270));
            g.drawRoundedRectangle (r.reduced (0.25f), 1.5f, 0.8f);
        }

    for (const auto& k : keys_)
        if (k.black)
        {
            const auto r = boundsFor (k).toFloat();
            const bool lit = lit_[k.note];
            const juce::Colour top = lit ? theme::jade : juce::Colour (0xff2e302c);
            const juce::Colour bot = lit ? theme::jadeDeep : juce::Colour (0xff141513);
            g.setGradientFill (juce::ColourGradient (top, r.getTopLeft(), bot, r.getBottomLeft(), false));
            g.fillRoundedRectangle (r, 1.5f);
            g.setColour (lit ? theme::jade : juce::Colour (0xff434640));
            g.drawRoundedRectangle (r.reduced (0.5f), 1.5f, 0.8f);
        }
}

void Keyboard::resized()
{
    repaint();
}

void Keyboard::mouseDown (const juce::MouseEvent& e)
{
    const int note = findNote (e.position);
    if (note >= 0)
    {
        activeMouseNote_ = note;
        setNoteOn (note, true);
        if (noteCallback_)
            noteCallback_ (note, true);
    }
}

void Keyboard::mouseDrag (const juce::MouseEvent& e)
{
    const int note = findNote (e.position);
    if (note == activeMouseNote_)
        return;

    if (activeMouseNote_ >= 0)
    {
        setNoteOn (activeMouseNote_, false);
        if (noteCallback_)
            noteCallback_ (activeMouseNote_, false);
    }

    activeMouseNote_ = note;
    if (activeMouseNote_ >= 0)
    {
        setNoteOn (activeMouseNote_, true);
        if (noteCallback_)
            noteCallback_ (activeMouseNote_, true);
    }
}

void Keyboard::mouseUp (const juce::MouseEvent&)
{
    if (activeMouseNote_ >= 0)
    {
        setNoteOn (activeMouseNote_, false);
        if (noteCallback_)
            noteCallback_ (activeMouseNote_, false);
        activeMouseNote_ = -1;
    }
}

// ============================================================================
// BankBrowser
// ============================================================================

BankBrowser::BankBrowser()
{
    addAndMakeVisible (bankCombo_);
    bankCombo_.setColour (juce::ComboBox::backgroundColourId, theme::displayBg);
    bankCombo_.setColour (juce::ComboBox::textColourId, theme::inkLight);
    bankCombo_.setColour (juce::ComboBox::arrowColourId, theme::jade);
    bankCombo_.setColour (juce::ComboBox::outlineColourId, theme::displayEdge);
    bankCombo_.setColour (juce::PopupMenu::backgroundColourId, theme::body2);
    bankCombo_.setColour (juce::PopupMenu::textColourId, theme::inkLight);
    bankCombo_.setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::jadeWash);
    bankCombo_.setColour (juce::PopupMenu::highlightedTextColourId, theme::jade);
    bankCombo_.setJustificationType (juce::Justification::centredLeft);
    bankCombo_.setTextWhenNothingSelected ("BANK --");
    bankCombo_.onChange = [this]
    {
        const int sel = bankCombo_.getSelectedId() - 1;
        if (sel >= 0 && sel < static_cast<int> (banks_.size()))
            filterFor (banks_[static_cast<std::size_t> (sel)]);
    };

    addAndMakeVisible (list_);
    list_.setOutlineThickness (0);
    list_.setColour (juce::ListBox::backgroundColourId, theme::displayBg);
    list_.setColour (juce::ListBox::outlineColourId, theme::displayEdge);
    list_.getViewport()->setScrollBarsShown (true, false);
    list_.getViewport()->setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    list_.getViewport()->setColour (juce::ScrollBar::trackColourId, theme::body2);
    list_.getViewport()->setColour (juce::ScrollBar::thumbColourId, theme::jadeDeep);
    list_.setRowHeight (22);

    addAndMakeVisible (emptyHint_);
    emptyHint_.setJustificationType (juce::Justification::centred);
    emptyHint_.setColour (juce::Label::textColourId, theme::inkMuted);
    emptyHint_.setFont (makeFont (10.5f, false));
    emptyHint_.setText ("Load a SoundFont (.sf2 / .sf3) to browse banks and programs",
                        juce::dontSendNotification);
}

BankBrowser::~BankBrowser() = default;

void BankBrowser::buildBanks()
{
    juce::SortedSet<int> seen;
    for (const auto& p : presets_)
        seen.add (p.bank);

    banks_.clear();
    bankCombo_.clear (juce::dontSendNotification);
    for (int bank : seen)
    {
        banks_.push_back (bank);
        bankCombo_.addItem ("BANK " + juce::String (bank).paddedLeft ('0', 3),
                            static_cast<int> (banks_.size()));
    }

    selectedBank_ = banks_.empty() ? -1 : banks_.front();
    if (!banks_.empty())
        bankCombo_.setSelectedId (1, juce::dontSendNotification);
    filterFor (selectedBank_);
}

void BankBrowser::filterFor (int bank)
{
    selectedBank_ = bank;
    filtered_.clear();
    for (int i = 0; i < static_cast<int> (presets_.size()); ++i)
        if (presets_[static_cast<std::size_t> (i)].bank == bank)
            filtered_.push_back (i);
    list_.updateContent();
    list_.deselectAllRows();
    list_.scrollToEnsureRowIsOnscreen (0);
    repaint();
}

void BankBrowser::setPresets (std::vector<PresetEntry> presets)
{
    presets_ = std::move (presets);
    buildBanks();
    list_.setVisible (!filtered_.empty());
    emptyHint_.setVisible (filtered_.empty());
}

void BankBrowser::setCurrent (int bank, int program)
{
    if (!banks_.empty())
    {
        for (std::size_t i = 0; i < banks_.size(); ++i)
        {
            if (banks_[i] == bank && bankCombo_.getSelectedId() != static_cast<int> (i + 1))
            {
                bankCombo_.setSelectedId (static_cast<int> (i + 1), juce::dontSendNotification);
                filterFor (bank);
                break;
            }
        }
    }

    for (std::size_t row = 0; row < filtered_.size(); ++row)
    {
        const auto& p = presets_[static_cast<std::size_t> (filtered_[row])];
        if (p.bank == bank && p.program == program)
        {
            if (list_.getSelectedRow() != static_cast<int> (row))
            {
                juce::SparseSet<int> selection;
                selection.addRange ({ static_cast<int> (row), static_cast<int> (row) + 1 });
                list_.setSelectedRows (selection, juce::dontSendNotification);
                list_.scrollToEnsureRowIsOnscreen (static_cast<int> (row));
            }
            return;
        }
    }
}

int BankBrowser::getNumRows()
{
    return static_cast<int> (filtered_.size());
}

void BankBrowser::paintListBoxItem (int rowNumber, juce::Graphics& g, int width,
                                    int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int> (filtered_.size()))
        return;

    const auto r = juce::Rectangle<int> (0, 0, width, height).reduced (2, 1).toFloat();
    const auto& p = presets_[static_cast<std::size_t> (filtered_[static_cast<std::size_t> (rowNumber)])];

    if (rowIsSelected)
    {
        g.setColour (theme::jadeWash);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (theme::jade);
        g.fillRect (r.getX(), r.getY() + 3.0f, 2.5f, r.getHeight() - 6.0f);
        g.setColour (theme::inkLight);
    }
    else
    {
        g.setColour ((rowNumber % 2 != 0) ? juce::Colour (0xff131916) : theme::displayBg);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (theme::inkLight.withAlpha (0.82f));
    }

    const auto num = juce::String (p.bank).paddedLeft ('0', 2) + ":"
                     + juce::String (p.program).paddedLeft ('0', 3);
    g.setFont (makeFont (9.5f, true));
    g.drawText (num, 10, 0, 58, height, juce::Justification::centredLeft, false);
    g.setFont (makeFont (10.0f, rowIsSelected));
    g.drawText (p.name, 70, 0, width - 78, height, juce::Justification::centredLeft, true);
}

void BankBrowser::selectedRowsChanged (int lastRowChanged)
{
    if (lastRowChanged < 0 || lastRowChanged >= static_cast<int> (filtered_.size()))
        return;
    const auto& p = presets_[static_cast<std::size_t> (filtered_[static_cast<std::size_t> (lastRowChanged)])];
    if (onSelect_)
        onSelect_ (p.bank, p.program);
}

void BankBrowser::resized()
{
    auto b = getLocalBounds();
    auto topBar = b.removeFromTop (24);
    bankCombo_.setBounds (topBar.removeFromLeft (132));
    b.removeFromTop (6);
    list_.setBounds (b);
    emptyHint_.setBounds (b);
}

// ============================================================================
// RomplerEditor
// ============================================================================

RomplerEditor::RomplerEditor (RomplerProcessor& processorRef)
    : juce::AudioProcessorEditor (processorRef),
      processor_ (processorRef),
      voiceBox_ ("VOICE", "SATURATION & FILTER"),
      busBox_ ("BUS", "TAPE, FOLD & OUTPUT"),
      fxBox_ ("FX", "CHORUS & REVERB", true),
      lnf_ (std::make_unique<DeckLookAndFeel>())
{
    setLookAndFeel (lnf_.get());

    addAndMakeVisible (brandTitle_);
    brandTitle_.setText ("EON-DS50", juce::dontSendNotification);
    brandTitle_.setJustificationType (juce::Justification::centredLeft);
    brandTitle_.setColour (juce::Label::textColourId, theme::inkLight);
    brandTitle_.setFont (makeFont (21.0f, true));

    addAndMakeVisible (brandSub_);
    brandSub_.setText ("SF2 ROMPLER  /  NONLINEAR DRIVE",
                       juce::dontSendNotification);
    brandSub_.setJustificationType (juce::Justification::centredLeft);
    brandSub_.setColour (juce::Label::textColourId, theme::jade);
    brandSub_.setFont (makeFont (9.0f, false));

    addAndMakeVisible (badges_);
    badges_.setJustificationType (juce::Justification::centredRight);
    badges_.setColour (juce::Label::textColourId, theme::inkMuted);
    badges_.setFont (makeFont (8.8f, false));
    badges_.setText ("VST3 / AU / STANDALONE\nSISYPHUS AUDIO",
                     juce::dontSendNotification);

    addAndMakeVisible (voiceBox_);
    addAndMakeVisible (busBox_);
    addAndMakeVisible (fxBox_);

    auto& apvts = processor_.getValueTreeState();
    controls_[0]  = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::voiceDrive)), true, false);
    controls_[1]  = std::make_unique<Switch>  (*dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::voiceCurve)));
    controls_[2]  = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::voiceVelToDrive)), false, true);
    controls_[3]  = std::make_unique<Toggle>  (*dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::voiceFilterRouting)));
    controls_[4]  = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::voiceFilterOffset)), false, true);
    controls_[5]  = std::make_unique<Stepper> (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::polyLimit)));
    controls_[6]  = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::busTapeDrive)), true, false);
    controls_[7]  = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::busFold)), true, false);
    controls_[8]  = std::make_unique<Switch>  (*dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::busOsFactor)), juce::String(), true);
    controls_[9]  = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::outTrim)), false, true);
    controls_[10] = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::outMix)));
    controls_[11] = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxChorusRate)));
    controls_[12] = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxChorusDepth)));
    controls_[13] = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxChorusMix)));
    controls_[14] = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxReverbRoom)));
    controls_[15] = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxReverbDamp)));
    controls_[16] = std::make_unique<Knob>    (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxReverbMix)));
    for (auto& c : controls_)
    {
        if (c)
            addAndMakeVisible (*c);
    }

    const auto setKnobLabel = [] (std::unique_ptr<juce::Component>& c, const juce::String& s)
    {
        if (auto* k = dynamic_cast<Knob*> (c.get()))
            k->setNameOverride (s);
    };
    const auto setChoiceLabel = [] (std::unique_ptr<juce::Component>& c, const juce::String& s)
    {
        if (auto* sw = dynamic_cast<Switch*> (c.get()))
            sw->setLabel (s);
        if (auto* t = dynamic_cast<Toggle*> (c.get()))
            t->setLabel (s);
        if (auto* st = dynamic_cast<Stepper*> (c.get()))
            st->setLabel (s);
    };
    setKnobLabel (controls_[0], "DRIVE");
    setChoiceLabel (controls_[1], "CURVE");
    setKnobLabel (controls_[2], "VEL > DRIVE");
    setChoiceLabel (controls_[3], "FILTER ROUTE");
    setKnobLabel (controls_[4], "FILTER OFFSET");
    setChoiceLabel (controls_[5], "POLYPHONY");
    setKnobLabel (controls_[6], "TAPE DRIVE");
    setKnobLabel (controls_[7], "FOLD");
    setChoiceLabel (controls_[8], "OVERSAMPLE");
    setKnobLabel (controls_[9], "OUT TRIM");
    setKnobLabel (controls_[10], "MIX");
    setKnobLabel (controls_[11], "CH RATE");
    setKnobLabel (controls_[12], "CH DEPTH");
    setKnobLabel (controls_[13], "CH MIX");
    setKnobLabel (controls_[14], "REV ROOM");
    setKnobLabel (controls_[15], "REV DAMP");
    setKnobLabel (controls_[16], "REV MIX");

    addAndMakeVisible (peakMeter_);

    // SoundFont Library toolbar + preset browser.
    addAndMakeVisible (sfLabel_);
    sfLabel_.setText ("SOUNDFONT", juce::dontSendNotification);
    sfLabel_.setJustificationType (juce::Justification::centredLeft);
    sfLabel_.setColour (juce::Label::textColourId, theme::jade);
    sfLabel_.setFont (makeFont (9.0f, true));

    addAndMakeVisible (sfDisplay_);
    sfDisplay_.setJustificationType (juce::Justification::centredLeft);
    sfDisplay_.setColour (juce::Label::textColourId, theme::inkLight);
    sfDisplay_.setColour (juce::Label::backgroundColourId, theme::displayBg);
    sfDisplay_.setColour (juce::Label::outlineColourId, theme::displayEdge);
    sfDisplay_.setFont (makeFont (10.0f, false));
    refreshDisplay();

    addAndMakeVisible (bankDigits_);
    bankDigits_.setJustificationType (juce::Justification::centred);
    bankDigits_.setColour (juce::Label::textColourId, theme::jade);
    bankDigits_.setColour (juce::Label::backgroundColourId, theme::displayBg);
    bankDigits_.setColour (juce::Label::outlineColourId, theme::displayEdge);
    bankDigits_.setFont (makeFont (10.0f, true));
    updateBankDigits();

    addAndMakeVisible (loadButton_);
    loadButton_.setButtonText ("LOAD SF2");
    loadButton_.setColour (juce::TextButton::buttonColourId, theme::jadeWash);
    loadButton_.setColour (juce::TextButton::buttonOnColourId, theme::jadeDeep);
    loadButton_.setColour (juce::TextButton::textColourOffId, theme::inkLight);
    loadButton_.setColour (juce::ComboBox::outlineColourId, theme::jadeDeep);
    loadButton_.onClick = [this] { onLoadButtonClicked(); };

    bankBrowser_ = std::make_unique<BankBrowser>();
    bankBrowser_->setOnSelect ([this] (int bank, int program) {
        processor_.selectPreset (bank, program);
        updateBankDigits();
    });
    addAndMakeVisible (*bankBrowser_);
    refreshPresetList();

    addAndMakeVisible (keyboard_);
    keyboard_.setKeyRange (36, 36);   // C2..B4
    keyboard_.setNoteCallback ([this] (int note, bool on) {
        processor_.postNote (note, on, 100);
    });

    setSize (820, 764);

    startTimerHz (20);
}

RomplerEditor::~RomplerEditor()
{
    setLookAndFeel (nullptr);
}

void RomplerEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::body2);

    const auto face = getLocalBounds().reduced (8).toFloat();
    g.setColour (theme::body);
    g.fillRoundedRectangle (face, 10.0f);
    g.setColour (theme::bodyEdge);
    g.drawRoundedRectangle (face.reduced (0.5f), 10.0f, 1.0f);

    // Restrained maker's seal beside the brand title.
    drawMakerSeal (g, juce::Rectangle<float> (18.0f, 22.0f, 22.0f, 22.0f));

    // Architectural hairline rule below the header with a celadon anchor bar.
    const float ruleY = 66.5f;
    g.setColour (theme::chassisRule);
    g.drawLine (16.0f, ruleY, static_cast<float> (getWidth()) - 16.0f, ruleY, 1.0f);
    g.setColour (theme::jadeDeep);
    g.fillRect (16.0f, ruleY - 0.5f, 44.0f, 1.5f);

    // Unified dark lacquer well for the SoundFont library & preset list.
    if (!libraryRect_.isEmpty())
    {
        const auto lib = libraryRect_.toFloat();
        g.setColour (juce::Colour (0xff141714));
        g.fillRoundedRectangle (lib, 6.0f);
        g.setColour (theme::bodyEdge);
        g.drawRoundedRectangle (lib.reduced (0.5f), 6.0f, 1.0f);
    }
}

void RomplerEditor::resized()
{
    auto b = getLocalBounds().reduced (16);

    // Header (y = 16..64).
    auto top = b.removeFromTop (48);
    auto brandCol = top.removeFromLeft (320).withTrimmedLeft (32);
    brandTitle_.setBounds (brandCol.removeFromTop (26));
    brandSub_.setBounds (brandCol.removeFromTop (16));
    badges_.setBounds (top.removeFromRight (200).reduced (0, 6));

    b.removeFromTop (14);

    // Upper deck: VOICE and BUS side by side.
    auto deck = b.removeFromTop (234);
    const int colW = (deck.getWidth() - 12) / 2;
    voiceBox_.setBounds (deck.getX(), deck.getY(), colW, deck.getHeight());
    busBox_.setBounds   (deck.getX() + colW + 12, deck.getY(), colW, deck.getHeight());

    layoutVoiceControls (voiceBox_.getBounds().reduced (10, 8).withTop (voiceBox_.getY() + 30));
    layoutBusControls   (busBox_.getBounds().reduced (10, 8).withTop (busBox_.getY() + 30));

    b.removeFromTop (12);

    // Middle deck: FX full width.
    auto fx = b.removeFromTop (132);
    fxBox_.setBounds (fx);
    layoutFxControls (fxBox_.getBounds().reduced (10, 8).withTop (fxBox_.getY() + 30));

    b.removeFromTop (12);

    // SoundFont library panel (toolbar + preset list).
    libraryRect_ = b.removeFromTop (168);
    auto libInner = libraryRect_.reduced (12, 10);
    auto toolbar = libInner.removeFromTop (26);
    sfLabel_.setBounds (toolbar.removeFromLeft (80));
    loadButton_.setBounds (toolbar.removeFromRight (88));
    toolbar.removeFromRight (8);
    bankDigits_.setBounds (toolbar.removeFromRight (78));
    toolbar.removeFromRight (8);
    sfDisplay_.setBounds (toolbar);

    libInner.removeFromTop (8);
    bankBrowser_->setBounds (libInner);

    b.removeFromTop (12);

    // Playable keyboard.
    keyboard_.setBounds (b);
}

void RomplerEditor::layoutVoiceControls (juce::Rectangle<int> area)
{
    const int cols = 3;
    const int rows = 2;
    const int cellW = area.getWidth() / cols;
    const int cellH = area.getHeight() / rows;

    auto place = [&] (int idx, int r, int c) {
        if (idx >= 0 && controls_[static_cast<std::size_t> (idx)])
            controls_[static_cast<std::size_t> (idx)]->setBounds (
                area.getX() + c * cellW, area.getY() + r * cellH, cellW, cellH);
    };
    place (0, 0, 0);   // Drive
    place (2, 0, 1);   // Vel -> Drive
    place (4, 0, 2);   // Filter Offset
    place (1, 1, 0);   // Curve
    place (3, 1, 1);   // Filter Route
    place (5, 1, 2);   // Polyphony
}

void RomplerEditor::layoutBusControls (juce::Rectangle<int> area)
{
    const int cols = 3;
    const int rows = 2;
    const int cellW = area.getWidth() / cols;
    const int cellH = area.getHeight() / rows;

    auto place = [&] (int idx, int r, int c) {
        if (idx >= 0 && controls_[static_cast<std::size_t> (idx)])
            controls_[static_cast<std::size_t> (idx)]->setBounds (
                area.getX() + c * cellW, area.getY() + r * cellH, cellW, cellH);
    };
    place (6, 0, 0);   // Tape Drive
    place (7, 0, 1);   // Fold
    place (8, 0, 2);   // Oversample
    place (9, 1, 0);   // Out Trim
    place (10, 1, 1);  // Mix
    peakMeter_.setBounds (area.getX() + 2 * cellW, area.getY() + cellH, cellW, cellH);
}

void RomplerEditor::layoutFxControls (juce::Rectangle<int> area)
{
    const int count = 6;
    const int cellW = area.getWidth() / count;
    for (int i = 0; i < count; ++i)
    {
        const int idx = 11 + i;
        if (controls_[static_cast<std::size_t> (idx)])
            controls_[static_cast<std::size_t> (idx)]->setBounds (
                area.getX() + i * cellW, area.getY(), cellW, area.getHeight());
    }
}

void RomplerEditor::refreshDisplay()
{
    const auto file = processor_.getLoadedFileName();
    lastLoadedFile_ = file;
    sfDisplay_.setText (file.isEmpty() ? "  No SoundFont loaded" : "  " + file,
                        juce::dontSendNotification);
}

void RomplerEditor::refreshPresetList()
{
    std::vector<PresetEntry> presets;
    const int count = processor_.getPresetCount();
    presets.reserve (static_cast<std::size_t> (count));
    for (int i = 0; i < count; ++i)
    {
        const auto [bank, program] = processor_.getPresetBankProgram (i);
        presets.push_back ({ bank, program, processor_.getPresetName (i) });
    }
    bankBrowser_->setPresets (std::move (presets));
    const auto [bank, program] = processor_.getCurrentBankProgram();
    bankBrowser_->setCurrent (bank, program);
    updateBankDigits();
}

void RomplerEditor::updateBankDigits()
{
    const auto [bank, program] = processor_.getCurrentBankProgram();
    bankDigits_.setText (juce::String (bank).paddedLeft ('0', 2) + " : "
                             + juce::String (program).paddedLeft ('0', 3),
                         juce::dontSendNotification);
}

void RomplerEditor::onLoadButtonClicked()
{
    fileChooser_ = std::make_unique<juce::FileChooser> (
        "Load a SoundFont...", juce::File (), "*.sf2;*.sf3");
    fileChooser_->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
            {
                processor_.loadSoundFont (file);
                refreshDisplay();
                refreshPresetList();
            }
            fileChooser_.reset();
        });
}

void RomplerEditor::comboBoxChanged (juce::ComboBox*)
{
}

void RomplerEditor::timerCallback()
{
    if (processor_.getLoadedFileName() != lastLoadedFile_)
    {
        refreshDisplay();
        refreshPresetList();
    }

    peakMeter_.setLevel (processor_.getLastPeak());

    updateBankDigits();
    const auto [bank, program] = processor_.getCurrentBankProgram();
    if (bankBrowser_)
        bankBrowser_->setCurrent (bank, program);
}

} // namespace aod
