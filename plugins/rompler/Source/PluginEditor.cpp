#include "PluginEditor.h"

#include <cmath>

namespace aod
{

namespace
{
// A few tiny paint helpers keep the hardware treatment consistent without
// introducing a global LookAndFeel or changing any control interaction.
void addBrushedGrain (juce::Graphics& g, juce::Rectangle<float> area, int seed);

void drawMachineScrew (juce::Graphics& g, juce::Point<float> centre, float radius)
{
    const auto body = juce::Rectangle<float> (centre.x - radius, centre.y - radius,
                                              radius * 2.0f, radius * 2.0f);
    g.setColour (juce::Colour (0x92000000));
    g.fillEllipse (body.translated (0.0f, 1.1f));

    juce::ColourGradient screw (juce::Colour (0xffb8bdb3), body.getTopLeft(),
                                juce::Colour (0xff3c4741), body.getBottomRight(), false);
    g.setGradientFill (screw);
    g.fillEllipse (body);
    g.setColour (juce::Colour (0x5cffffff));
    g.drawEllipse (body.reduced (0.45f), 0.65f);
    // A tiny cross-head and its raised leading edge make the fasteners read
    // as hardware rather than decorative dots, even at the plug-in scale.
    g.setColour (juce::Colour (0xae000000));
    g.drawLine (centre.x - radius * 0.45f, centre.y,
                centre.x + radius * 0.45f, centre.y, 0.82f);
    g.drawLine (centre.x, centre.y - radius * 0.45f,
                centre.x, centre.y + radius * 0.45f, 0.82f);
    g.setColour (juce::Colour (0x3effffff));
    g.drawLine (centre.x - radius * 0.38f, centre.y - 0.42f,
                centre.x + radius * 0.38f, centre.y - 0.42f, 0.38f);
    g.drawLine (centre.x - 0.42f, centre.y - radius * 0.38f,
                centre.x - 0.42f, centre.y + radius * 0.38f, 0.38f);
}

void drawChassisRail (juce::Graphics& g, juce::Rectangle<float> rail, int seed)
{
    g.setColour (juce::Colour (0x9a000000));
    g.fillRoundedRectangle (rail.translated (0.0f, 1.6f), 4.0f);
    juce::ColourGradient railGrad (juce::Colour (0xff748178), rail.getTopLeft(),
                                   juce::Colour (0xff18211e), rail.getBottomRight(), false);
    g.setGradientFill (railGrad);
    g.fillRoundedRectangle (rail, 4.0f);
    g.setColour (juce::Colour (0x55ffffff));
    g.drawRoundedRectangle (rail.reduced (0.55f), 3.5f, 0.68f);
    g.setColour (juce::Colour (0x79000000));
    g.drawLine (rail.getCentreX() + 1.0f, rail.getY() + 4.0f,
                rail.getCentreX() + 1.0f, rail.getBottom() - 4.0f, 1.0f);
    addBrushedGrain (g, rail.reduced (1.0f), seed);
    drawMachineScrew (g, { rail.getCentreX(), rail.getY() + 10.0f }, 2.15f);
    drawMachineScrew (g, { rail.getCentreX(), rail.getBottom() - 10.0f }, 2.15f);
}

void addBrushedGrain (juce::Graphics& g, juce::Rectangle<float> area, int seed)
{
    g.saveState();
    g.reduceClipRegion (area.toNearestInt());
    const auto top = juce::roundToInt (area.getY()) + 3;
    const auto bottom = juce::roundToInt (area.getBottom()) - 2;
    for (int y = top; y < bottom; y += 5)
    {
        const int phase = (y * 17 + seed * 31) % 11;
        g.setColour (juce::Colour (0xffffffff).withAlpha (0.016f + 0.005f * (float) phase));
        g.drawHorizontalLine (y, area.getX() + 2.0f, area.getRight() - 2.0f);

        // Short, offset strokes stop the grain reading as a uniform web
        // gradient or scanline texture.
        if ((y + seed) % 3 == 0)
        {
            const float x = area.getX() + 9.0f + (float) ((y * 19 + seed) % 37);
            g.setColour (juce::Colour (0xff000000).withAlpha (0.055f));
            g.drawLine (x, (float) y + 1.0f, std::min (x + 20.0f, area.getRight() - 3.0f),
                        (float) y + 1.0f, 0.55f);
        }
    }
    g.restoreState();
}

bool presetStateEqual (const PresetDocument& a, const PresetDocument& b)
{
    if (a.soundFontPath != b.soundFontPath || a.bank != b.bank || a.program != b.program
        || a.parameters.size() != b.parameters.size())
        return false;

    for (std::size_t i = 0; i < a.parameters.size(); ++i)
    {
        const auto& lhs = a.parameters[i];
        const auto& rhs = b.parameters[i];
        if (lhs.id != rhs.id || lhs.isChoice != rhs.isChoice || lhs.text != rhs.text
            || std::abs (lhs.value - rhs.value) > 0.0001f)
            return false;
    }
    return true;
}
} // namespace

// ============================================================================
// SectionBox
// ============================================================================

SectionBox::SectionBox (const juce::String& title)
{
    addAndMakeVisible (title_);
    title_.setText (title, juce::dontSendNotification);
    title_.setJustificationType (juce::Justification::centred);
    title_.setColour (juce::Label::textColourId, juce::Colour (0xff071016));
    title_.setFont (makeFont (11.0f, true));
    // Keep the tab fully inside the component bounds; drawing at y=-8 gets
    // clipped by JUCE's child-component paint region on some hosts.
    title_.setBounds (10, 2, 80, 16);
}

SectionBox::~SectionBox() = default;

void SectionBox::paint (juce::Graphics& g)
{
    // The functional modules sit in their own raised, slightly worn inserts:
    // outer chassis shadow -> alloy bezel -> dark recessed control cavity.
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xa6000000));
    g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), 9.0f);

    juce::ColourGradient bezelGrad (juce::Colour (0xff718078), bounds.getTopLeft(),
                                    juce::Colour (0xff101716), bounds.getBottomLeft(), false);
    g.setGradientFill (bezelGrad);
    g.fillRoundedRectangle (bounds, 9.0f);
    g.setColour (juce::Colour (0x5efffff8));
    g.drawRoundedRectangle (bounds.reduced (0.55f), 8.4f, 0.9f);

    const auto cavity = bounds.reduced (3.0f);
    g.setColour (juce::Colour (0xff050909));
    g.fillRoundedRectangle (cavity, 6.5f);
    const auto panelFace = cavity.reduced (1.0f);
    juce::ColourGradient panelGrad (theme::panel, panelFace.getTopLeft(),
                                    juce::Colour (0xff25302c), panelFace.getBottomLeft(), false);
    g.setGradientFill (panelGrad);
    g.fillRoundedRectangle (panelFace, 5.6f);
    g.setColour (juce::Colour (0x55ffffff));
    g.drawRoundedRectangle (panelFace.reduced (0.35f), 5.3f, 0.7f);
    g.setColour (juce::Colour (0x75000000));
    g.drawRoundedRectangle (panelFace.reduced (1.1f).translated (0.0f, 0.7f), 4.9f, 1.1f);
    // Double scribe lines mimic the shallow tooling marks around a real
    // replaceable module faceplate and visually separate the control surface
    // from the surrounding chassis.
    g.setColour (juce::Colour (0x26000000));
    g.drawRoundedRectangle (panelFace.reduced (4.0f), 3.5f, 0.65f);
    g.setColour (juce::Colour (0x20ffffff));
    g.drawRoundedRectangle (panelFace.reduced (4.7f).translated (-0.25f, -0.25f), 2.9f, 0.45f);
    addBrushedGrain (g, panelFace.reduced (1.5f), getWidth() + getHeight());

    // Small coloured tabs identify sections without flooding every border in
    // UI mint.  The child label is painted on top as an engraved legend.
    const auto tab = juce::Rectangle<int> (10, 2, 80, 16);
    const auto tabBounds = tab.toFloat();
    g.setColour (juce::Colour (0x8a000000));
    g.fillRoundedRectangle (tabBounds.translated (0.0f, 1.2f), 3.5f);
    juce::ColourGradient tabGrad (juce::Colour (0xff9fd9c4), tabBounds.getTopLeft(),
                                 juce::Colour (0xff347d68), tabBounds.getBottomLeft(), false);
    g.setGradientFill (tabGrad);
    g.fillRoundedRectangle (tabBounds, 3.5f);
    g.setColour (juce::Colour (0x55ffffff));
    g.drawRoundedRectangle (tabBounds.reduced (0.45f), 3.0f, 0.7f);

    drawMachineScrew (g, { bounds.getX() + 9.0f, bounds.getY() + 9.0f }, 2.4f);
    drawMachineScrew (g, { bounds.getRight() - 9.0f, bounds.getY() + 9.0f }, 2.4f);
    drawMachineScrew (g, { bounds.getRight() - 9.0f, bounds.getBottom() - 9.0f }, 2.4f);
    drawMachineScrew (g, { bounds.getX() + 9.0f, bounds.getBottom() - 9.0f }, 2.4f);
}

// ============================================================================
// HardwareButton
// ============================================================================

void HardwareButton::paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const float press = isButtonDown ? 1.0f : 0.0f;
    g.setColour (juce::Colour (0xaa000000));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 5.0f);

    juce::ColourGradient bezel (juce::Colour (0xff738078), bounds.getTopLeft(),
                                juce::Colour (0xff111817), bounds.getBottomLeft(), false);
    g.setGradientFill (bezel);
    g.fillRoundedRectangle (bounds, 5.0f);

    const auto cap = bounds.reduced (2.0f).translated (0.0f, press);
    juce::ColourGradient key (isMouseOverButton ? juce::Colour (0xfffbf8e8) : theme::knobCream,
                             cap.getTopLeft(), juce::Colour (0xffb9c1b4), cap.getBottomLeft(), false);
    g.setGradientFill (key);
    g.fillRoundedRectangle (cap, 3.5f);
    g.setColour (juce::Colour (0x6efffff4));
    g.drawRoundedRectangle (cap.reduced (0.45f), 3.0f, 0.8f);
    g.setColour (juce::Colour (0x7a000000));
    g.drawRoundedRectangle (cap.reduced (0.9f).translated (0.0f, 0.5f), 2.8f, 0.7f);

    g.setFont (makeFont (11.0f, true));
    g.setColour (juce::Colour (0x5a000000));
    g.drawText (getButtonText(), cap.translated (0.0f, 0.8f), juce::Justification::centred);
    g.setColour (juce::Colour (0xff17201e));
    g.drawText (getButtonText(), cap, juce::Justification::centred);
}

// ============================================================================
// Knob
// ============================================================================

Knob::Knob (juce::RangedAudioParameter& param, bool hot)
    : param_ (param), hot_ (hot)
{
    slider_.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider_.setDoubleClickReturnValue (false, 0.5, true);
    slider_.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    attachment_ = std::make_unique<juce::SliderParameterAttachment> (param, slider_);

    // The slider is not a visible child (we draw the knob ourselves), so value
    // changes that arrive through the attachment never repaint us. Registering
    // as a parameter listener makes the pointer and value print track the
    // parameter from any source: drag, wheel, host automation, preset load.
    param_.addListener (this);

    addAndMakeVisible (name_);
    name_.setText (param.getName (32), juce::dontSendNotification);
    name_.setJustificationType (juce::Justification::centred);
    name_.setColour (juce::Label::textColourId, theme::ink);
    name_.setFont (makeFont (10.5f, true));

    addAndMakeVisible (value_);
    value_.setJustificationType (juce::Justification::centred);
    value_.setColour (juce::Label::textColourId, theme::inkSoft);
    value_.setFont (makeDisplayFont (11.0f, true));

    // Printed panel legends stay beneath their knobs, as on a hardware
    // synth. Only the live numeric readout is transient while adjusting.
    name_.setVisible (true);
    value_.setVisible (false);

    setSize (70, 96);
}

Knob::~Knob()
{
    // The parameter (owned by the processor) outlives this widget, so we must
    // unregister before destruction or a later host change would call back into
    // freed memory.
    param_.removeListener (this);
}

void Knob::syncFromParameter()
{
    const auto& range = param_.getNormalisableRange();
    slider_.setValue (range.convertFrom0to1 (param_.getValue()), juce::dontSendNotification);
    refreshInitState();
    setReadoutVisible (false);
    repaint();
}

void Knob::refreshInitState()
{
    // Compare in normalised space: this remains stable if a parameter's
    // engineering range changes, and mirrors the value used by the control's
    // display. A tiny epsilon absorbs float conversion at range endpoints.
    constexpr float epsilon = 0.0001f;
    const float current = param_.convertTo0to1 (static_cast<float> (slider_.getValue()));
    atInit_ = std::abs (current - param_.getDefaultValue()) <= epsilon;
}

void Knob::parameterValueChanged (int, float)
{
    // JUCE calls this synchronously and may do so from the audio thread.
    // repaint() must run on the message thread, so just request an async
    // update and let handleAsyncUpdate() do the drawing.
    triggerAsyncUpdate();
}

void Knob::handleAsyncUpdate()
{
    refreshInitState();
    repaint();
}

void Knob::timerCallback()
{
    stopTimer();
    setReadoutVisible (false);
}

void Knob::setReadoutVisible (bool shouldBeVisible)
{
    if (readoutVisible_ == shouldBeVisible)
        return;

    readoutVisible_ = shouldBeVisible;
    value_.setText (slider_.getTextFromValue (slider_.getValue()), juce::dontSendNotification);
    value_.setVisible (shouldBeVisible);
    repaint();
}

void Knob::paint (juce::Graphics& g)
{
    const float w = (float) getWidth();
    const float h = (float) getHeight();
    const float capDiameter = std::min (50.0f, std::max (24.0f, std::min (w - 12.0f, h - 30.0f)));
    const auto capArea = juce::Rectangle<float> (0.0f, 13.0f, w, std::max (0.0f, h - 28.0f));
    const auto unpressedCap = capArea.withSizeKeepingCentre (capDiameter, capDiameter);
    const float press = pressed_ ? 1.0f : 0.0f;
    const auto cap = unpressedCap.translated (0.0f, press);
    const auto well = unpressedCap.expanded (5.1f);
    const auto centre = cap.getCentre();
    const float radius = cap.getWidth() * 0.5f;
    const float unit = juce::jlimit (0.0f, 1.0f,
                                     static_cast<float> (param_.convertTo0to1 (static_cast<float> (slider_.getValue()))));
    const auto accent = (hot_ ? theme::hot : theme::mint);
    const auto initAccent = theme::mintGlow;
    const auto activeAccent = atInit_ ? initAccent : accent;

    // A deep, asymmetric socket makes the cap read as a physical part resting
    // in a cut-out rather than a flat rotary glyph.
    juce::ColourGradient wellGradient (juce::Colour (0xff26332f), well.getTopLeft(),
                                       theme::knobWell, well.getBottomRight(), false);
    g.setGradientFill (wellGradient);
    g.fillEllipse (well.translated (0.0f, 2.35f));
    g.setColour (juce::Colour (0xbc000000));
    g.drawEllipse (well.translated (0.0f, 2.35f), 1.35f);
    g.setColour (juce::Colour (0x306f7c73));
    g.drawEllipse (well.reduced (1.6f).translated (-0.55f, -0.65f), 0.65f);

    // The lower ellipse is a visible moulded sidewall; keeping it darker than
    // the face gives the dial a low, reassuringly heavy phenolic cap profile.
    juce::ColourGradient sideGradient (juce::Colour (0xff58675f), cap.getTopLeft(),
                                       theme::knobSide, cap.getBottomLeft(), false);
    g.setGradientFill (sideGradient);
    g.fillEllipse (cap.translated (0.0f, 3.65f - press * 0.45f));
    g.setColour (juce::Colour (0x9c000000));
    g.drawEllipse (cap.translated (0.0f, 3.65f - press * 0.45f).reduced (0.4f), 1.0f);
    g.setColour (juce::Colour (0x3effffff));
    juce::Path sideHighlight;
    sideHighlight.addArc (cap.getX() + 1.1f, cap.getY() + 2.7f, cap.getWidth() - 2.2f, cap.getHeight() - 2.2f,
                          juce::degreesToRadians (8.0f), juce::degreesToRadians (172.0f), true);
    g.strokePath (sideHighlight, juce::PathStrokeType (0.58f));

    juce::ColourGradient capGradient (theme::knobCapHi.withAlpha (0.94f), cap.getTopLeft(),
                                      theme::knobCap, cap.getBottomRight(), false);
    g.setGradientFill (capGradient);
    g.fillEllipse (cap);
    g.setColour (theme::knobRim.withAlpha (0.70f));
    g.drawEllipse (cap.reduced (0.65f), 0.9f);
    g.setColour (juce::Colour (0x47000000));
    g.drawEllipse (cap.reduced (2.25f), 1.25f);
    // Concentric chamfers reproduce the shallow moulded step found on a
    // weighted synth cap.  They also keep the top from reading as a flat icon.
    g.setColour (juce::Colour (0x28ffffff));
    g.drawEllipse (cap.reduced (4.1f).translated (-0.35f, -0.5f), 0.58f);
    g.setColour (juce::Colour (0x4b000000));
    juce::Path innerChamfer;
    innerChamfer.addArc (cap.getX() + 4.0f, cap.getY() + 4.0f, cap.getWidth() - 8.0f, cap.getHeight() - 8.0f,
                         juce::degreesToRadians (12.0f), juce::degreesToRadians (168.0f), true);
    g.strokePath (innerChamfer, juce::PathStrokeType (0.72f));

    // Grip ribs live on the exposed sidewall only.  Omitting the upper arc
    // keeps the lit cap face clean instead of giving it a radial-sunburst UI
    // appearance.
    for (int i = 0; i < 22; ++i)
    {
        const float ridgeAngle = juce::MathConstants<float>::twoPi * (float) i / 22.0f;
        const float dx = std::cos (ridgeAngle);
        const float dy = std::sin (ridgeAngle);
        if (dy < -0.18f)
            continue;
        const float innerRadius = radius - 1.1f;
        const float outerRadius = radius + 2.5f;
        const juce::Point<float> inner (centre.x + dx * innerRadius, centre.y + dy * innerRadius);
        const juce::Point<float> outer (centre.x + dx * outerRadius, centre.y + dy * outerRadius);
        g.setColour (juce::Colour (0x82000000));
        g.drawLine (inner.x, inner.y + 0.65f, outer.x, outer.y + 0.65f, 1.15f);
        g.setColour (juce::Colour (0x2dffffff));
        g.drawLine (inner.x - dx * 0.45f, inner.y - dy * 0.45f,
                    outer.x - dx * 0.45f, outer.y - dy * 0.45f, 0.48f);
    }

    // A small, off-centre upper-left bloom gives the moulded surface a light
    // direction while preserving a matte instrument-panel finish.
    g.setColour (juce::Colour (0x2dffffff));
    g.fillEllipse (cap.reduced (7.0f).translated (-2.2f, -2.8f));
    g.setColour (juce::Colour (0x16ffffff));
    g.fillEllipse (cap.reduced (12.0f).translated (-2.6f, -3.2f));

    // 264-degree workstation scale, centred at 12 o'clock. Bipolar controls
    // illuminate from the centre detent; unipolar controls fill from minimum.
    const auto range = param_.getNormalisableRange();
    const bool isBipolar = range.start < 0.0f && range.end > 0.0f;
    constexpr int tickCount = 13;
    constexpr int centreTick = tickCount / 2;
    const int currentTick = juce::jlimit (0, tickCount - 1,
                                          juce::roundToInt (unit * (float) (tickCount - 1)));
    const float scaleRadius = radius + 6.4f;
    for (int i = 0; i < tickCount; ++i)
    {
        const float degrees = -222.0f + (264.0f * (float) i / (float) (tickCount - 1));
        const float angle = juce::degreesToRadians (degrees);
        const float dx = std::cos (angle);
        const float dy = std::sin (angle);
        const bool major = i == 0 || i == 3 || i == centreTick || i == 9 || i == tickCount - 1;
        const bool active = isBipolar
            ? (i >= std::min (centreTick, currentTick) && i <= std::max (centreTick, currentTick))
            : i <= currentTick;
        const float tickLength = major ? 4.75f : 2.75f;
        const float tickWidth = major ? 1.1f : 0.75f;
        const juce::Point<float> outer (centre.x + dx * (scaleRadius + 1.0f),
                                        centre.y + dy * (scaleRadius + 1.0f));
        const juce::Point<float> inner (centre.x + dx * (scaleRadius - tickLength),
                                        centre.y + dy * (scaleRadius - tickLength));
        g.setColour ((active ? activeAccent : theme::inkSoft).withAlpha (active ? 0.80f : (major ? 0.43f : 0.22f)));
        g.drawLine (inner.x, inner.y, outer.x, outer.y, tickWidth);
    }

    if (isBipolar)
    {
        const float detentAngle = juce::degreesToRadians (-90.0f);
        const juce::Point<float> detent (centre.x + std::cos (detentAngle) * (scaleRadius + 3.0f),
                                         centre.y + std::sin (detentAngle) * (scaleRadius + 3.0f));
        g.setColour (theme::knobPointer.withAlpha (0.70f));
        g.fillEllipse (detent.x - 1.4f, detent.y - 1.4f, 2.8f, 2.8f);
    }

    // A short ivory insert sits inside a narrow black slot, mimicking a real
    // cap indicator rather than a line painted across the centre.
    const float pointerAngle = juce::degreesToRadians (-222.0f + unit * 264.0f);
    const float pointerDx = std::cos (pointerAngle);
    const float pointerDy = std::sin (pointerAngle);
    const float pointerStart = radius * 0.36f;
    const float pointerEnd = radius - 5.1f;
    const juce::Point<float> pointerA (centre.x + pointerDx * pointerStart,
                                       centre.y + pointerDy * pointerStart);
    const juce::Point<float> pointerB (centre.x + pointerDx * pointerEnd,
                                       centre.y + pointerDy * pointerEnd);
    g.setColour (juce::Colour (0xc8000000));
    g.drawLine (pointerA.x, pointerA.y + 1.05f, pointerB.x, pointerB.y + 1.05f, 3.55f);
    g.setColour ((hot_ ? juce::Colour (0xfffff0df) : theme::knobPointer).withAlpha (0.96f));
    g.drawLine (pointerA.x, pointerA.y, pointerB.x, pointerB.y, 1.82f);
    g.setColour (activeAccent.withAlpha (0.42f));
    g.drawLine (pointerA.x, pointerA.y - 0.28f, pointerB.x, pointerB.y - 0.28f, 0.52f);

    g.setColour (juce::Colour (0x8c000000));
    g.fillEllipse (centre.x - 3.7f, centre.y - 3.25f, 7.4f, 7.4f);
    g.setColour (theme::knobCapHi.withAlpha (0.48f));
    g.fillEllipse (centre.x - 2.35f, centre.y - 2.65f, 4.7f, 4.7f);

    g.setColour (activeAccent.withAlpha (0.18f + unit * 0.26f));
    g.drawEllipse (cap.expanded (1.55f), hot_ ? 1.25f : 0.95f);
    g.setColour (juce::Colour (0x25000000));
    juce::Path socketShadow;
    socketShadow.addArc (well.getX(), well.getY() + 1.2f, well.getWidth(), well.getHeight(),
                         juce::degreesToRadians (18.0f), juce::degreesToRadians (162.0f), true);
    g.strokePath (socketShadow, juce::PathStrokeType (1.0f));

    value_.setText (slider_.getTextFromValue (slider_.getValue()), juce::dontSendNotification);
}

void Knob::resized()
{
    const auto w = getWidth();
    const auto h = getHeight();
    // The fixed legend is physically printed below the control. Its live
    // numeric counterpart takes the upper legend position only while moved.
    value_.setBounds (getLocalBounds().withSizeKeepingCentre (w, 14).withY (0));
    name_.setBounds (getLocalBounds().withSizeKeepingCentre (w, 15).withY (h - 15));
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    stopTimer();
    pressed_ = true;
    setReadoutVisible (true);
    lastDragY_ = e.y;
    setMouseCursor (juce::MouseCursor::IBeamCursor);
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    pressed_ = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    setReadoutVisible (false);
    refreshInitState();
    repaint();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    // Map a full-height drag to the parameter's whole range so the knob is
    // actually usable. The slider stores denormalised parameter values (e.g.
    // Drive is 0..100), so a raw per-pixel delta of 1/60 is far too small to
    // move it visibly.
    const auto range = param_.getNormalisableRange();
    const float pixelsPerFullRange = 200.0f;
    const float delta = (lastDragY_ - e.y) * (range.end - range.start) / pixelsPerFullRange;
    lastDragY_ = e.y;
    slider_.setValue (slider_.getValue() + delta, juce::sendNotificationSync);
}

void Knob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    setReadoutVisible (true);
    startTimer (650);
    const auto range = param_.getNormalisableRange();
    const float step = (range.end - range.start) / 100.0f;
    slider_.setValue (slider_.getValue() + (float) wheel.deltaY * step, juce::sendNotificationSync);
}

// ============================================================================
// Switch (segmented buttons)
// ============================================================================

Switch::Switch (juce::AudioParameterChoice& param, const juce::String& label, bool leds)
    : param_ (param), leds_ (leds)
{
    addAndMakeVisible (label_);
    label_.setText (label.isEmpty() ? param.getName (32).toUpperCase() : label, juce::dontSendNotification);
    label_.setJustificationType (juce::Justification::centred);
    label_.setColour (juce::Label::textColourId, theme::ink);
    label_.setFont (makeFont (10.5f, true));

    for (const auto& choice : param.getAllValueStrings())
        box_.addItem (choice, box_.getNumItems() + 1);
    attachment_ = std::make_unique<juce::ComboBoxParameterAttachment> (param, box_);

    setSize (110, 64);
}

Switch::~Switch() = default;

void Switch::resized()
{
    const auto w = getWidth();
    const int pillW = juce::jlimit (48, w - 8, 96);
    const int top = leds_ ? 2 : 4;
    pill_ = juce::Rectangle<int> (0, top, pillW, leds_ ? 20 : 24).withX ((w - pillW) / 2);
    label_.setBounds (getLocalBounds().withSizeKeepingCentre (w, 14).withY (pill_.getBottom() + (leds_ ? 2 : 4)));
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
    auto b = pill_.toFloat();
    // Hardware selector: dark bezel, sunk cavity, then a shallow illuminated
    // rocker.  This keeps seafoam as a backlight rather than a web-button fill.
    const auto bezel = b.expanded (2.0f);
    g.setColour (juce::Colour (0x9a000000));
    g.fillRoundedRectangle (bezel.translated (0.0f, 1.5f), 6.0f);
    juce::ColourGradient bezelGrad (juce::Colour (0xff6e7d74), bezel.getTopLeft(),
                                    juce::Colour (0xff101716), bezel.getBottomLeft(), false);
    g.setGradientFill (bezelGrad);
    g.fillRoundedRectangle (bezel, 6.0f);
    g.setColour (juce::Colour (0xff060a0a));
    g.fillRoundedRectangle (b, 4.0f);

    const auto rocker = b.reduced (1.4f);
    juce::ColourGradient segGrad (juce::Colour (0xff65c7aa), rocker.getTopLeft(),
                                  juce::Colour (0xff246b59), rocker.getBottomLeft(), false);
    g.setGradientFill (segGrad);
    g.fillRoundedRectangle (rocker, 3.2f);
    g.setColour (juce::Colour (0x5cffffff));
    g.drawRoundedRectangle (rocker.reduced (0.35f), 2.8f, 0.65f);
    g.setColour (juce::Colour (0x70000000));
    g.drawRoundedRectangle (rocker.reduced (0.8f).translated (0.0f, 0.6f), 2.5f, 0.7f);
    g.setColour (theme::mintGlow.withAlpha (0.18f));
    g.fillRoundedRectangle (rocker.withHeight (1.25f), 1.0f);

    if (leds_)
    {
        const int n = 5;
        const float gap = 3.0f;
        const float pad = 8.0f;
        const float ledW = (getWidth() - 2 * pad - gap * (n - 1)) / (float) n;
        const float ledH = 16.0f;
        const float ledY = b.getBottom() + 4.0f;
        const auto ledWell = juce::Rectangle<float> (pad - 3.0f, ledY - 2.5f,
                                                      getWidth() - 2.0f * pad + 6.0f, ledH + 5.0f);
        g.setColour (juce::Colour (0xa6000000));
        g.fillRoundedRectangle (ledWell, 4.0f);
        g.setColour (juce::Colour (0x3dffffff));
        g.drawRoundedRectangle (ledWell.reduced (0.4f), 3.5f, 0.65f);
        for (int i = 0; i < n; ++i)
        {
            auto r = juce::Rectangle<float> (pad + i * (ledW + gap), ledY, ledW, ledH);
            const auto colour = (i == n - 1) ? theme::ledRed
                             : (i == n - 2) ? theme::ledHot
                                            : theme::ledMint;
            g.setColour (theme::ledOff);
            g.fillRoundedRectangle (r, 2.0f);
            g.setColour (colour.withAlpha (0.78f));
            g.fillRoundedRectangle (r.reduced (1.05f, 1.4f), 1.25f);
            g.setColour (juce::Colour (0x48ffffff));
            g.fillRoundedRectangle (r.reduced (1.8f, 2.1f).withHeight (1.0f), 0.6f);
        }
    }

    const int idx = box_.getSelectedItemIndex();
    juce::String name = (idx >= 0 && idx + 1 <= box_.getNumItems())
                        ? box_.getItemText (idx + 1).toUpperCase()
                        : juce::String();
    g.setColour (theme::knobCream);
    g.setFont (makeFont (10.0f, true));
    g.drawText (name, b, juce::Justification::centred);
}

void Switch::mouseDown (const juce::MouseEvent& e)
{
    lastDragY_ = e.y;
    setMouseCursor (juce::MouseCursor::IBeamCursor);
}

void Switch::mouseUp (const juce::MouseEvent& e)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    if (std::abs (lastDragY_ - e.y) < 5.0f)
        advanceChoice();
}

void Switch::mouseDrag (const juce::MouseEvent& e)
{
    const float dy = lastDragY_ - e.y;
    if (std::abs (dy) > 24.0f)
    {
        lastDragY_ = e.y;
        advanceChoice();
    }
}

void Switch::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY > 0)
    {
        const int n = box_.getNumItems();
        const int cur = box_.getSelectedId();
        box_.setSelectedId (cur <= 1 ? n : cur - 1, juce::sendNotificationSync);
    }
    else if (wheel.deltaY < 0)
        advanceChoice();
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
    label_.setFont (makeFont (10.5f, true));

    for (const auto& choice : param.getAllValueStrings())
        box_.addItem (choice, box_.getNumItems() + 1);
    attachment_ = std::make_unique<juce::ComboBoxParameterAttachment> (param, box_);

    setSize (110, 64);
}

Toggle::~Toggle() = default;

void Toggle::resized()
{
    const auto w = getWidth();
    label_.setBounds (getLocalBounds().withSizeKeepingCentre (w, 14).withY (32));
}

void Toggle::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const int idx = box_.getSelectedItemIndex();

    const float trackW = 44.0f;
    const float trackH = 20.0f;
    auto track = juce::Rectangle<float> ((b.getWidth() - trackW) * 0.5f, 6.0f, trackW, trackH);

    const bool on = (idx == 1);

    // A physical slide-switch has a deep black slot, a small chrome/phenolic
    // carriage and a status lens; it is deliberately not a flat mobile toggle.
    const auto bezel = track.expanded (2.3f);
    g.setColour (juce::Colour (0x98000000));
    g.fillRoundedRectangle (bezel.translated (0.0f, 1.6f), 7.0f);
    juce::ColourGradient bezelGrad (juce::Colour (0xff6f7d74), bezel.getTopLeft(),
                                    juce::Colour (0xff101716), bezel.getBottomLeft(), false);
    g.setGradientFill (bezelGrad);
    g.fillRoundedRectangle (bezel, 7.0f);
    g.setColour (juce::Colour (0xff050909));
    g.fillRoundedRectangle (track, trackH * 0.5f);

    const auto slot = track.reduced (1.7f, 2.15f);
    juce::ColourGradient slotGrad (on ? juce::Colour (0xff2e7865) : juce::Colour (0xff2a302e),
                                   slot.getTopLeft(),
                                   on ? juce::Colour (0xff0c2d25) : juce::Colour (0xff0b1010),
                                   slot.getBottomLeft(), false);
    g.setGradientFill (slotGrad);
    g.fillRoundedRectangle (slot, slot.getHeight() * 0.5f);
    g.setColour (juce::Colour (0x55ffffff));
    g.drawRoundedRectangle (slot.reduced (0.35f), slot.getHeight() * 0.5f - 0.35f, 0.65f);

    const float thumbD = 14.0f;
    const float thumbX = on ? track.getX() + track.getWidth() - thumbD - 2.0f : track.getX() + 2.0f;
    const auto thumb = juce::Rectangle<float> (thumbX, track.getY() + 2.0f, thumbD, thumbD);
    g.setColour (juce::Colour (0x9e000000));
    g.fillEllipse (thumb.translated (0.0f, 1.15f));
    juce::ColourGradient thumbGrad (theme::knobCream, thumb.getTopLeft(),
                                    juce::Colour (0xff949c92), thumb.getBottomRight(), false);
    g.setGradientFill (thumbGrad);
    g.fillEllipse (thumb);
    g.setColour (juce::Colour (0x72ffffff));
    g.drawEllipse (thumb.reduced (0.65f), 0.75f);
    g.setColour (juce::Colour (0x80000000));
    g.drawLine (thumb.getCentreX() - 3.0f, thumb.getCentreY() + 0.8f,
                thumb.getCentreX() + 3.0f, thumb.getCentreY() + 0.8f, 0.85f);

    const float lensX = on ? track.getRight() - 3.5f : track.getX() + 3.5f;
    g.setColour ((on ? theme::ledMint : theme::ledOff).withAlpha (0.60f));
    g.fillEllipse (lensX - 1.5f, track.getCentreY() - 1.5f, 3.0f, 3.0f);

    // Pre / Post labels either side.
    g.setColour (on ? theme::inkSoft : theme::knobCream);
    g.setFont (makeFont (8.8f, true));
    g.drawText ("PRE",  juce::roundToInt (track.getX()) - 30, 8, 26, 16, juce::Justification::centred);
    g.drawText ("POST", juce::roundToInt (track.getRight()) + 4, 8, 30, 16, juce::Justification::centred);
}

void Toggle::mouseDown (const juce::MouseEvent&)
{
    box_.setSelectedId ((box_.getSelectedId() == 1) ? 2 : 1, juce::sendNotificationSync);
    repaint();
}

// ============================================================================
// PitchWheel / ModWheel
// ============================================================================

namespace
{
    // Shared paint helper for the two performance wheels: a recessed slot
    // with a travelling chrome/phenolic carriage, in the same hardware
    // language as Toggle's slide switch. `fillFraction` is the carriage
    // position in [0, 1] from the bottom of the slot; `centreTick` draws a
    // detent mark at the spring-to-centre position (pitch wheel only).
    void paintWheelSlot (juce::Graphics& g, juce::Rectangle<float> track, float fillFraction,
                         bool showCentreTick, juce::Colour litColour)
    {
        const auto bezel = track.expanded (2.3f, 3.0f);
        g.setColour (juce::Colour (0x98000000));
        g.fillRoundedRectangle (bezel.translated (0.0f, 1.6f), 7.0f);
        juce::ColourGradient bezelGrad (juce::Colour (0xff6f7d74), bezel.getTopLeft(),
                                        juce::Colour (0xff101716), bezel.getBottomRight(), false);
        g.setGradientFill (bezelGrad);
        g.fillRoundedRectangle (bezel, 7.0f);

        g.setColour (juce::Colour (0xff050909));
        g.fillRoundedRectangle (track, track.getWidth() * 0.5f);

        const auto slot = track.reduced (2.15f, 1.7f);
        juce::ColourGradient slotGrad (juce::Colour (0xff2a302e), slot.getTopLeft(),
                                       juce::Colour (0xff0b1010), slot.getBottomLeft(), false);
        g.setGradientFill (slotGrad);
        g.fillRoundedRectangle (slot, slot.getWidth() * 0.5f);
        g.setColour (juce::Colour (0x55ffffff));
        g.drawRoundedRectangle (slot.reduced (0.35f), slot.getWidth() * 0.5f - 0.35f, 0.65f);

        if (showCentreTick)
        {
            const float tickY = slot.getCentreY();
            g.setColour (juce::Colour (0x60ffffff));
            g.drawLine (slot.getX() - 2.0f, tickY, slot.getRight() + 2.0f, tickY, 0.8f);
        }

        const float thumbD = slot.getWidth() + 6.0f;
        const float travel = slot.getHeight() - thumbD;
        const float thumbY = slot.getBottom() - thumbD - fillFraction * travel;
        const auto thumb = juce::Rectangle<float> (slot.getCentreX() - thumbD * 0.5f, thumbY, thumbD, thumbD);
        g.setColour (juce::Colour (0x9e000000));
        g.fillEllipse (thumb.translated (0.0f, 1.15f));
        juce::ColourGradient thumbGrad (theme::knobCream, thumb.getTopLeft(),
                                        juce::Colour (0xff949c92), thumb.getBottomRight(), false);
        g.setGradientFill (thumbGrad);
        g.fillEllipse (thumb);
        g.setColour (juce::Colour (0x72ffffff));
        g.drawEllipse (thumb.reduced (0.65f), 0.75f);
        g.setColour (juce::Colour (0x80000000));
        g.drawLine (thumb.getX() + 3.0f, thumb.getCentreY(),
                    thumb.getRight() - 3.0f, thumb.getCentreY(), 0.85f);

        if (fillFraction > 0.02f)
        {
            g.setColour (litColour.withAlpha (juce::jlimit (0.0f, 0.75f, fillFraction * 0.8f + 0.1f)));
            g.fillEllipse (thumb.getCentreX() - 1.6f, thumb.getCentreY() - 1.6f, 3.2f, 3.2f);
        }
    }
}

PitchWheel::PitchWheel (std::function<void (float)> onChange)
    : onChange_ (std::move (onChange))
{
    addAndMakeVisible (label_);
    label_.setText ("PITCH", juce::dontSendNotification);
    label_.setJustificationType (juce::Justification::centred);
    label_.setColour (juce::Label::textColourId, theme::inkSoft);
    label_.setFont (makeFont (8.8f, true));
    setSize (46, 168);
}

PitchWheel::~PitchWheel() = default;

void PitchWheel::resized()
{
    label_.setBounds (getLocalBounds().removeFromBottom (14));
}

void PitchWheel::paint (juce::Graphics& g)
{
    auto track = getLocalBounds().toFloat().withTrimmedBottom (16.0f).reduced (14.0f, 4.0f);
    // fillFraction in [0,1] maps value_ in [-1,1], centred at 0.5.
    const float fillFraction = (value_ + 1.0f) * 0.5f;
    paintWheelSlot (g, track, fillFraction, true, theme::ledMint);
}

void PitchWheel::mouseDown (const juce::MouseEvent& e)
{
    dragging_ = true;
    dragStartY_ = e.position.y;
    dragStartValue_ = value_;
}

void PitchWheel::mouseDrag (const juce::MouseEvent& e)
{
    const float travel = static_cast<float> (getHeight()) - 32.0f;
    const float delta = (dragStartY_ - e.position.y) / juce::jmax (1.0f, travel);
    value_ = juce::jlimit (-1.0f, 1.0f, dragStartValue_ + delta * 2.0f);
    if (onChange_)
        onChange_ (value_);
    repaint();
}

void PitchWheel::mouseUp (const juce::MouseEvent&)
{
    dragging_ = false;
    value_ = 0.0f;
    if (onChange_)
        onChange_ (value_);
    repaint();
}

void PitchWheel::setValue (float normalizedValue)
{
    if (dragging_)
        return;
    const float clamped = juce::jlimit (-1.0f, 1.0f, normalizedValue);
    if (! juce::approximatelyEqual (clamped, value_))
    {
        value_ = clamped;
        repaint();
    }
}

ModWheel::ModWheel (std::function<void (float)> onChange)
    : onChange_ (std::move (onChange))
{
    addAndMakeVisible (label_);
    label_.setText ("MOD", juce::dontSendNotification);
    label_.setJustificationType (juce::Justification::centred);
    label_.setColour (juce::Label::textColourId, theme::inkSoft);
    label_.setFont (makeFont (8.8f, true));
    setSize (46, 168);
}

ModWheel::~ModWheel() = default;

void ModWheel::resized()
{
    label_.setBounds (getLocalBounds().removeFromBottom (14));
}

void ModWheel::paint (juce::Graphics& g)
{
    auto track = getLocalBounds().toFloat().withTrimmedBottom (16.0f).reduced (14.0f, 4.0f);
    paintWheelSlot (g, track, value_, false, theme::ledMint);
}

void ModWheel::mouseDown (const juce::MouseEvent& e)
{
    dragging_ = true;
    dragStartY_ = e.position.y;
    dragStartValue_ = value_;
}

void ModWheel::mouseDrag (const juce::MouseEvent& e)
{
    const float travel = static_cast<float> (getHeight()) - 32.0f;
    const float delta = (dragStartY_ - e.position.y) / juce::jmax (1.0f, travel);
    value_ = juce::jlimit (0.0f, 1.0f, dragStartValue_ + delta);
    if (onChange_)
        onChange_ (value_);
    repaint();
}

void ModWheel::mouseUp (const juce::MouseEvent&)
{
    // Mod wheel holds its position on release, unlike the pitch wheel.
    dragging_ = false;
}

void ModWheel::setValue (float normalizedValue)
{
    if (dragging_)
        return;
    const float clamped = juce::jlimit (0.0f, 1.0f, normalizedValue);
    if (! juce::approximatelyEqual (clamped, value_))
    {
        value_ = clamped;
        repaint();
    }
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
    label_.setFont (makeFont (10.5f, true));

    slider_.setRange (param.getNormalisableRange().start, param.getNormalisableRange().end, 1.0);
    slider_.setValue (param.getValue(), juce::dontSendNotification);
    attachment_ = std::make_unique<juce::SliderParameterAttachment> (param, slider_);

    // See Knob: the hidden slider's value changes do not repaint the readout
    // on their own, so listen to the parameter directly.
    param_.addListener (this);

    setSize (110, 64);
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
    const auto w = getWidth();
    label_.setBounds (getLocalBounds().withSizeKeepingCentre (w, 14).withY (36));
}

void Stepper::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const int value = juce::roundToInt (slider_.getValue());

    auto digits = juce::Rectangle<float> ((b.getWidth() - 48.0f) * 0.5f, 2.0f, 48.0f, 28.0f);
    const auto bezel = digits.expanded (2.6f, 2.2f);
    g.setColour (juce::Colour (0xaa000000));
    g.fillRoundedRectangle (bezel.translated (0.0f, 1.5f), 4.8f);
    juce::ColourGradient bezelGrad (juce::Colour (0xff6f7d74), bezel.getTopLeft(),
                                    juce::Colour (0xff101716), bezel.getBottomLeft(), false);
    g.setGradientFill (bezelGrad);
    g.fillRoundedRectangle (bezel, 4.8f);

    // Smoked workstation LCD: its phosphor sits below the bezel rather than
    // appearing as a mint button.
    juce::ColourGradient lcdGrad (theme::displayFg, digits.getTopLeft(),
                                  theme::displayBg, digits.getBottomLeft(), false);
    g.setGradientFill (lcdGrad);
    g.fillRoundedRectangle (digits, 3.0f);
    g.setColour (theme::displayOn.withAlpha (0.46f));
    g.drawRoundedRectangle (digits.reduced (0.5f), 2.5f, 0.75f);
    for (float y = digits.getY() + 4.0f; y < digits.getBottom() - 2.0f; y += 5.0f)
    {
        g.setColour (juce::Colour (0x16000000));
        g.drawHorizontalLine (juce::roundToInt (y), digits.getX() + 2.0f, digits.getRight() - 2.0f);
    }
    g.setColour (juce::Colour (0x80000000));
    g.setFont (makeDisplayFont (14.5f, true));
    g.drawText (juce::String (value).paddedLeft ('0', 2), digits.translated (0.0f, 0.8f), juce::Justification::centred);
    g.setColour (theme::displayOn);
    g.setFont (makeDisplayFont (14.5f, true));
    g.drawText (juce::String (value).paddedLeft ('0', 2), digits, juce::Justification::centred);
}

void Stepper::mouseDown (const juce::MouseEvent& e)
{
    lastDragY_ = e.y;
    setMouseCursor (juce::MouseCursor::IBeamCursor);
}

void Stepper::mouseUp (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void Stepper::mouseDrag (const juce::MouseEvent& e)
{
    // Same scaling as Knob: a full-height drag spans the parameter's range, so
    // integer parameters (polyphony 1..128) advance visibly instead of 1 per
    // 60 pixels.
    const auto range = param_.getNormalisableRange();
    const float pixelsPerFullRange = 200.0f;
    const float delta = (lastDragY_ - e.y) * (range.end - range.start) / pixelsPerFullRange;
    lastDragY_ = e.y;
    slider_.setValue (slider_.getValue() + delta, juce::sendNotificationSync);
}

void Stepper::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    slider_.setValue (slider_.getValue() + (float) wheel.deltaY, juce::sendNotificationSync);
}

// ============================================================================
// PeakMeter
// ============================================================================

PeakMeter::PeakMeter() = default;
PeakMeter::~PeakMeter() = default;

void PeakMeter::setLevel (float level)
{
    level_ = juce::jlimit (0.0f, 1.0f, level);
    repaint();
}

void PeakMeter::paint (juce::Graphics& g)
{
    const auto well = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (0xa8000000));
    g.fillRoundedRectangle (well.translated (0.0f, 1.1f), 4.0f);
    juce::ColourGradient bezel (juce::Colour (0xff46514b), well.getTopLeft(),
                                juce::Colour (0xff070b0b), well.getBottomLeft(), false);
    g.setGradientFill (bezel);
    g.fillRoundedRectangle (well, 4.0f);
    const auto inner = well.reduced (2.0f, 3.0f);
    g.setColour (juce::Colour (0xff060a0a));
    g.fillRoundedRectangle (inner, 2.5f);

    const float w = inner.getWidth();
    const float h = inner.getHeight();
    const float gap = 3.0f;
    const float ledW = (w - gap * (numSegments - 1)) / (float) numSegments;
    const int lit = juce::roundToInt (level_ * (float) numSegments);

    for (int i = 0; i < numSegments; ++i)
    {
        auto r = juce::Rectangle<float> (inner.getX() + i * (ledW + gap), inner.getY(), ledW, h);
        const bool on = i < lit;
        const auto colour = (i >= numSegments - 1) ? theme::ledRed
                          : (i >= numSegments - 3) ? theme::ledHot
                                                   : theme::ledMint;
        g.setColour (theme::ledOff);
        g.fillRoundedRectangle (r, 2.0f);
        if (on)
        {
            juce::ColourGradient ledGrad (colour.brighter (0.10f), r.getTopLeft(),
                                          colour.darker (0.18f), r.getBottomLeft(), false);
            g.setGradientFill (ledGrad);
            g.fillRoundedRectangle (r.reduced (0.9f, 1.1f), 1.1f);
            g.setColour (juce::Colour (0x4effffff));
            g.fillRoundedRectangle (r.reduced (1.5f, 1.65f).withHeight (0.8f), 0.4f);
        }
    }
}

// ============================================================================
// GainReductionMeter
// ============================================================================

GainReductionMeter::GainReductionMeter() = default;
GainReductionMeter::~GainReductionMeter() = default;

void GainReductionMeter::setReductionDb (float reductionDb)
{
    reductionDb_ = juce::jlimit (0.0f, 24.0f, reductionDb);
    repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (0xa8000000));
    g.fillRoundedRectangle (bounds.translated (0.0f, 1.0f), 3.8f);
    juce::ColourGradient bezel (juce::Colour (0xff455049), bounds.getTopLeft(),
                                juce::Colour (0xff070b0b), bounds.getBottomLeft(), false);
    g.setGradientFill (bezel);
    g.fillRoundedRectangle (bounds, 3.8f);
    const auto inner = bounds.reduced (2.0f, 2.0f);
    g.setColour (juce::Colour (0xff060a0a));
    g.fillRoundedRectangle (inner, 2.0f);
    constexpr float labelWidth = 16.0f;
    constexpr float gap = 2.0f;
    const auto ledArea = inner.withTrimmedLeft (labelWidth + 3.0f);
    const float ledWidth = (ledArea.getWidth() - gap * (float) (numSegments - 1)) / (float) numSegments;
    const int lit = juce::jlimit (0, numSegments,
                                  juce::roundToInt ((reductionDb_ / 24.0f) * (float) numSegments));

    g.setColour (theme::inkSoft.withAlpha (0.86f));
    g.setFont (makeDisplayFont (8.8f, true));
    g.drawText ("GR", inner.withWidth (labelWidth), juce::Justification::centredLeft);

    for (int i = 0; i < numSegments; ++i)
    {
        const auto segment = juce::Rectangle<float> (ledArea.getX() + i * (ledWidth + gap),
                                                      ledArea.getY() + 2.0f,
                                                      ledWidth,
                                                      std::max (2.0f, ledArea.getHeight() - 4.0f));
        const auto colour = i >= numSegments - 1 ? theme::ledRed
                          : i >= numSegments - 3 ? theme::ledHot
                                                  : theme::ledMint;
        g.setColour (theme::ledOff);
        g.fillRoundedRectangle (segment, 1.5f);
        if (i < lit)
        {
            juce::ColourGradient ledGrad (colour.brighter (0.08f), segment.getTopLeft(),
                                          colour.darker (0.18f), segment.getBottomLeft(), false);
            g.setGradientFill (ledGrad);
            g.fillRoundedRectangle (segment.reduced (0.55f, 0.8f), 0.9f);
            g.setColour (colour.withAlpha (0.26f));
            g.drawRoundedRectangle (segment.expanded (0.5f), 1.8f, 0.8f);
        }
    }
}

// ============================================================================
// Keyboard
// ============================================================================

Keyboard::Keyboard()
{
    setSize (800, 120);
}

Keyboard::~Keyboard() = default;

void Keyboard::setKeyRange (int lowNote, int numKeys)
{
    keys_.clear();
    lit_.clear();
    numWhite_ = 0;

    // Chromatic black-key pattern: C#=1, D#=3, F#=6, G#=8, A#=10
    static constexpr int isBlack[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
    for (int i = 0; i < numKeys; ++i)
    {
        const int note = lowNote + i;
        const int pc = note % 12;
        if (isBlack[pc])
            keys_.push_back ({ note, true, numWhite_ - 1 });
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
    const auto b = getLocalBounds();
    const float whiteW = (numWhite_ > 0) ? (float) b.getWidth() / (float) numWhite_ : 0.0f;

    if (!k.black)
    {
        const int x = static_cast<int> (k.whiteIndex * whiteW);
        return { x, b.getY(), static_cast<int> (std::lround (whiteW)), b.getHeight() };
    }

    const int blackW = static_cast<int> (whiteW * 0.62f);
    const int x = static_cast<int> ((k.whiteIndex + 1) * whiteW) - blackW / 2;
    const int blackH = static_cast<int> (b.getHeight() * 0.62f);
    return { x, b.getY(), blackW, blackH };
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
    const auto bed = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff080d0d));
    g.setColour (juce::Colour (0xae000000));
    g.fillRoundedRectangle (bed.reduced (0.5f).translated (0.0f, 2.0f), 5.0f);
    juce::ColourGradient rail (juce::Colour (0xff465049), bed.getTopLeft(),
                               juce::Colour (0xff080c0c), bed.getBottomLeft(), false);
    g.setGradientFill (rail);
    g.fillRoundedRectangle (bed.reduced (0.5f), 5.0f);
    g.setColour (juce::Colour (0x55ffffff));
    g.drawRoundedRectangle (bed.reduced (1.05f), 4.5f, 0.72f);
    g.setColour (juce::Colour (0x64000000));
    g.fillRoundedRectangle (bed.reduced (2.0f).withHeight (4.0f).translated (0.0f, 1.0f), 1.6f);
    const auto keyBed = bed.reduced (3.0f, 3.0f);
    g.setColour (juce::Colour (0xff111716));
    g.fillRoundedRectangle (keyBed, 3.0f);

    for (const auto& k : keys_)
        if (!k.black)
        {
            auto r = boundsFor (k).toFloat().reduced (0.75f, 0.0f)
                                                .withTrimmedTop (5.0f)
                                                .withTrimmedBottom (3.0f);
            const bool lit = lit_[k.note];
            g.setColour (juce::Colour (0x94000000));
            g.fillRoundedRectangle (r.translated (0.0f, 2.4f), 2.4f);
            juce::Colour top = lit ? juce::Colour (0xffcce5d9) : theme::knobCream;
            juce::Colour bot = lit ? juce::Colour (0xff56917f) : juce::Colour (0xff9ca39a);
            g.setGradientFill (juce::ColourGradient (top, r.getTopLeft(), bot, r.getBottomLeft(), false));
            g.fillRoundedRectangle (r, 2.0f);
            g.setColour (juce::Colour (0x82ffffff));
            g.drawLine (r.getX() + 1.0f, r.getY() + 0.8f, r.getRight() - 1.0f, r.getY() + 0.8f, 0.75f);
            g.setColour (juce::Colour (0x75000000));
            g.drawRoundedRectangle (r.reduced (0.35f), 1.7f, 0.8f);
            g.setColour (juce::Colour (0x18ffffff));
            g.drawLine (r.getX() + 2.0f, r.getY() + 4.0f, r.getX() + 2.0f, r.getBottom() - 8.0f, 0.5f);

            const auto lip = r.withY (r.getBottom() - 5.0f).withHeight (5.0f);
            juce::ColourGradient lipGrad (juce::Colour (0x34000000), lip.getTopLeft(),
                                          juce::Colour (0x8c000000), lip.getBottomLeft(), false);
            g.setGradientFill (lipGrad);
            g.fillRect (lip);
            if (lit)
            {
                g.setColour (theme::mintGlow.withAlpha (0.86f));
                g.fillRect (lip.withHeight (1.5f));
            }
        }

    for (const auto& k : keys_)
        if (k.black)
        {
            auto r = boundsFor (k).toFloat().reduced (0.8f, 0.0f)
                                                .withTrimmedTop (4.0f)
                                                .withTrimmedBottom (2.0f);
            const bool lit = lit_[k.note];
            g.setColour (juce::Colour (0xb6000000));
            g.fillRoundedRectangle (r.translated (0.0f, 2.4f), 2.4f);
            juce::Colour top = lit ? juce::Colour (0xff3f8f7c) : juce::Colour (0xff424b46);
            juce::Colour bot = lit ? juce::Colour (0xff0d332a) : juce::Colour (0xff050808);
            g.setGradientFill (juce::ColourGradient (top, r.getTopLeft(), bot, r.getBottomLeft(), false));
            g.fillRoundedRectangle (r, 2.4f);
            g.setColour (juce::Colour (0x52ffffff));
            g.drawLine (r.getX() + 1.5f, r.getY() + 1.0f, r.getRight() - 1.5f, r.getY() + 1.0f, 0.75f);
            g.setColour (juce::Colour (0x3a000000));
            g.drawLine (r.getRight() - 1.3f, r.getY() + 3.0f, r.getRight() - 1.3f, r.getBottom() - 2.5f, 0.6f);
            if (lit)
            {
                g.setColour (theme::mintGlow.withAlpha (0.82f));
                g.fillRoundedRectangle (r.withY (r.getBottom() - 2.5f).withHeight (1.4f), 0.6f);
            }
        }
}

void Keyboard::resized()
{
    repaint();
}

void Keyboard::mouseDown (const juce::MouseEvent& e)
{
    if (const int note = findNote (e.position))
    {
        setNoteOn (note, true);
        if (noteCallback_)
            noteCallback_ (note, true);
    }
}

void Keyboard::mouseUp (const juce::MouseEvent& e)
{
    if (const int note = findNote (e.position))
    {
        setNoteOn (note, false);
        if (noteCallback_)
            noteCallback_ (note, false);
    }
}

// ============================================================================
// BankBrowser
// ============================================================================

BankBrowser::BankBrowser()
{
    addAndMakeVisible (bankCombo_);
    bankCombo_.setColour (juce::ComboBox::backgroundColourId, theme::displayBg);
    bankCombo_.setColour (juce::ComboBox::textColourId, theme::displayOn);
    bankCombo_.setColour (juce::ComboBox::arrowColourId, theme::displayOn.withAlpha (0.75f));
    bankCombo_.setColour (juce::PopupMenu::backgroundColourId, theme::body2);
    bankCombo_.setColour (juce::PopupMenu::textColourId, theme::knobCream);
    bankCombo_.setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::mintDeep);
    bankCombo_.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff06231b));
    bankCombo_.setJustificationType (juce::Justification::centred);
    bankCombo_.setTextWhenNothingSelected ("BANK");
    bankCombo_.setTextWhenNoChoicesAvailable ("NO BANKS");
    bankCombo_.setColour (juce::ComboBox::outlineColourId, theme::displayOn.withAlpha (0.42f));
    bankCombo_.setColour (juce::ComboBox::buttonColourId, theme::displayFg);
    bankCombo_.onChange = [this]
    {
        const int sel = bankCombo_.getSelectedId() - 1;
        if (sel >= 0 && sel < (int) banks_.size())
            filterFor (banks_[static_cast<std::size_t> (sel)]);
    };

    addAndMakeVisible (list_);
    list_.setOutlineThickness (0);
    list_.setColour (juce::ListBox::backgroundColourId, theme::displayBg);
    list_.setColour (juce::ListBox::outlineColourId, theme::mintDeep);
    list_.getViewport()->setScrollBarsShown (true, false);
    list_.getViewport()->setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    list_.getViewport()->setColour (juce::ScrollBar::trackColourId, theme::bodyEdge);
    list_.getViewport()->setColour (juce::ScrollBar::thumbColourId, theme::mintDeep);
    list_.setRowHeight (24);

    addAndMakeVisible (emptyHint_);
    emptyHint_.setJustificationType (juce::Justification::centred);
    emptyHint_.setColour (juce::Label::textColourId, theme::mint);
    emptyHint_.setFont (makeFont (12.0f, false));
    emptyHint_.setText ("NO SOUNDFONT LOADED - PRESS LOAD TO BROWSE PRESETS",
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
    for (int b : seen)
    {
        banks_.push_back (b);
        bankCombo_.addItem (juce::String (b).paddedLeft ('0', 2), (int) banks_.size());
    }

    selectedBank_ = banks_.empty() ? -1 : banks_.front();
    if (!banks_.empty())
        bankCombo_.setSelectedId (1, juce::dontSendNotification);
    else
        bankCombo_.setText ("BANK", juce::dontSendNotification);
    filterFor (selectedBank_);
}

void BankBrowser::filterFor (int bank)
{
    selectedBank_ = bank;
    filtered_.clear();
    for (int i = 0; i < (int) presets_.size(); ++i)
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
    // Move the bank strip to the requested bank, then highlight its program.
    if (!banks_.empty())
    {
        for (std::size_t i = 0; i < banks_.size(); ++i)
        {
            if (banks_[i] == bank && bankCombo_.getSelectedId() != (int) (i + 1))
            {
                bankCombo_.setSelectedId ((int) (i + 1), juce::dontSendNotification);
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
            if (list_.getSelectedRow() != (int) row)
            {
                juce::SparseSet<int> selection;
                selection.addRange ({ (int) row, (int) row + 1 });
                list_.setSelectedRows (selection, juce::dontSendNotification);
                list_.scrollToEnsureRowIsOnscreen ((int) row);
            }
            return;
        }
    }
}

int BankBrowser::getNumRows()
{
    return (int) filtered_.size();
}

void BankBrowser::paintListBoxItem (int rowNumber, juce::Graphics& g, int width,
                                    int height, bool rowIsSelected)
{
    auto r = juce::Rectangle<int> (0, 0, width, height).reduced (2, 1).toFloat();
    if (rowNumber < 0 || rowNumber >= (int) filtered_.size())
        return;
    const auto& p = presets_[static_cast<std::size_t> (filtered_[static_cast<std::size_t> (rowNumber)])];

    if (rowIsSelected)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff346f5e), r.getTopLeft(),
                                                 juce::Colour (0xff12372e), r.getBottomLeft(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colour (0xffeff1e4));
    }
    else
    {
        g.setColour ((rowNumber % 2) ? juce::Colour (0xff111817) : juce::Colour (0xff0b1110));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (theme::ink);
    }

    const auto num = juce::String (p.bank).paddedLeft ('0', 2) + "-"
                     + juce::String (p.program).paddedLeft ('0', 3);
    g.setFont (makeDisplayFont (10.8f, true));
    g.drawText (num, 6, 0, 56, height, juce::Justification::centredLeft);
    g.setFont (makeFont (10.8f, false));
    g.drawText (p.name, 64, 0, width - 70, height, juce::Justification::centredLeft);
}

void BankBrowser::selectedRowsChanged (int lastRowChanged)
{
    if (lastRowChanged < 0 || lastRowChanged >= (int) filtered_.size())
        return;
    const auto& p = presets_[static_cast<std::size_t> (filtered_[static_cast<std::size_t> (lastRowChanged)])];
    if (onSelect_)
        onSelect_ (p.bank, p.program);
}

void BankBrowser::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (0xa5000000));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 7.5f);
    juce::ColourGradient bezel (juce::Colour (0xff515c54), bounds.getTopLeft(),
                                juce::Colour (0xff070c0c), bounds.getBottomLeft(), false);
    g.setGradientFill (bezel);
    g.fillRoundedRectangle (bounds, 7.5f);
    const auto screen = bounds.reduced (3.0f);
    juce::ColourGradient grad (theme::displayFg, screen.getTopLeft(),
                               theme::displayBg, screen.getBottomLeft(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (screen, 5.0f);
    g.setColour (theme::displayOn.withAlpha (0.35f));
    g.drawRoundedRectangle (screen.reduced (0.35f), 4.6f, 0.8f);
    // A restrained diagonal reflection makes this read as a smoked hardware
    // LCD behind glass rather than a plain flat list background.
    juce::Path reflection;
    reflection.startNewSubPath (screen.getX() + 5.0f, screen.getY() + 2.0f);
    reflection.lineTo (screen.getX() + screen.getWidth() * 0.52f, screen.getY() + 2.0f);
    reflection.lineTo (screen.getX() + screen.getWidth() * 0.26f, screen.getBottom() - 3.0f);
    reflection.lineTo (screen.getX() + 3.0f, screen.getBottom() - 3.0f);
    reflection.closeSubPath();
    g.setColour (juce::Colour (0x0cffffff));
    g.fillPath (reflection);
    for (float y = screen.getY() + 7.0f; y < screen.getBottom() - 2.0f; y += 7.0f)
    {
        g.setColour (juce::Colour (0x10000000));
        g.drawHorizontalLine (juce::roundToInt (y), screen.getX() + 2.0f, screen.getRight() - 2.0f);
    }
}

void BankBrowser::resized()
{
    auto b = getLocalBounds();
    bankCombo_.setBounds (b.removeFromTop (26).withTrimmedLeft (2).withTrimmedRight (2));
    b.removeFromTop (4);
    list_.setBounds (b.reduced (2, 0));
    emptyHint_.setBounds (b.reduced (2, 0));
}

// ============================================================================
// EnvelopeGraph
// ============================================================================

EnvelopeGraph::EnvelopeGraph (juce::RangedAudioParameter& attack,
                              juce::RangedAudioParameter& decay,
                              juce::RangedAudioParameter& sustain,
                              juce::RangedAudioParameter& release)
    : params_ { &attack, &decay, &sustain, &release }
{
    for (size_t i = 0; i < params_.size(); ++i)
    {
        sliders_[i].setRange (params_[i]->getNormalisableRange().start,
                              params_[i]->getNormalisableRange().end, 0.01);
        sliders_[i].setValue (params_[i]->getValue(), juce::dontSendNotification);
        attachments_[i] = std::make_unique<juce::SliderParameterAttachment> (*params_[i], sliders_[i]);
        params_[i]->addListener (this);
    }
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

EnvelopeGraph::~EnvelopeGraph()
{
    for (auto* p : params_)
        p->removeListener (this);
}

juce::Point<float> EnvelopeGraph::pointFor (int stage, juce::Rectangle<float> area) const
{
    const float a = static_cast<float> (sliders_[0].getValue() / 100.0);
    const float d = static_cast<float> (sliders_[1].getValue() / 100.0);
    const float s = static_cast<float> (sliders_[2].getValue() / 100.0);
    const float r = static_cast<float> (sliders_[3].getValue() / 100.0);
    const float x1 = area.getX() + area.getWidth() * 0.04f;
    const float x2 = x1 + area.getWidth() * (0.14f + a * 0.18f);
    const float x3 = x2 + area.getWidth() * (0.18f + d * 0.20f);
    const float x4 = area.getRight() - area.getWidth() * (0.08f + r * 0.16f);
    const float yTop = area.getY() + area.getHeight() * 0.12f;
    const float ySus = area.getBottom() - area.getHeight() * (0.12f + s * 0.62f);
    const float yBase = area.getBottom() - area.getHeight() * 0.12f;
    switch (stage)
    {
        case 0: return { x1, yBase };
        case 1: return { x2, yTop };
        case 2: return { x3, ySus };
        default: return { x4, yBase };
    }
}

void EnvelopeGraph::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (10.0f, 4.0f);
    const auto bezel = area.expanded (2.0f);
    g.setColour (juce::Colour (0x96000000));
    g.fillRoundedRectangle (bezel.translated (0.0f, 1.4f), 6.0f);
    juce::ColourGradient bezelGrad (juce::Colour (0xff455049), bezel.getTopLeft(),
                                    juce::Colour (0xff070c0c), bezel.getBottomLeft(), false);
    g.setGradientFill (bezelGrad);
    g.fillRoundedRectangle (bezel, 6.0f);
    juce::ColourGradient screenGrad (theme::displayFg, area.getTopLeft(),
                                     theme::displayBg, area.getBottomLeft(), false);
    g.setGradientFill (screenGrad);
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (theme::displayOn.withAlpha (0.38f));
    g.drawRoundedRectangle (area.reduced (0.5f), 4.4f, 0.75f);
    for (float y = area.getY() + 7.0f; y < area.getBottom() - 4.0f; y += 7.0f)
    {
        g.setColour (juce::Colour (0x14000000));
        g.drawHorizontalLine (juce::roundToInt (y), area.getX() + 2.0f, area.getRight() - 2.0f);
    }

    for (int i = 1; i < 4; ++i)
    {
        const auto p = pointFor (i, area);
        g.setColour (theme::displayOn.withAlpha (0.13f));
        g.drawVerticalLine (juce::roundToInt (p.x), area.getY() + 2.0f, area.getBottom() - 2.0f);
    }

    juce::Path curve;
    curve.startNewSubPath (pointFor (0, area));
    curve.lineTo (pointFor (1, area));
    curve.lineTo (pointFor (2, area));
    curve.lineTo (pointFor (3, area));
    g.setColour (theme::mintGlow.withAlpha (0.15f));
    g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
    g.setColour (theme::displayOn);
    g.strokePath (curve, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved));

    static constexpr const char* names[] = { "A", "D", "S", "R" };
    for (int i = 0; i < 4; ++i)
    {
        const auto p = pointFor (i, area);
        g.setColour (juce::Colour (0x9a000000));
        g.fillEllipse (p.x - 5.0f, p.y - 4.0f, 10.0f, 10.0f);
        juce::ColourGradient pointGrad (i == activeStage_ ? theme::hot : theme::knobCream,
                                        p.translated (-4.0f, -4.0f),
                                        i == activeStage_ ? theme::hotDeep : juce::Colour (0xff9da39a),
                                        p.translated (4.0f, 4.0f), false);
        g.setGradientFill (pointGrad);
        g.fillEllipse (p.x - 4.5f, p.y - 5.0f, 9.0f, 9.0f);
        g.setColour (juce::Colour (0x68ffffff));
        g.drawEllipse (p.x - 4.1f, p.y - 4.6f, 8.2f, 8.2f, 0.65f);
        g.setColour (theme::ink);
        g.drawText (names[i], juce::Rectangle<float> (p.x - 10.0f, area.getBottom() - 14.0f, 20.0f, 12.0f), juce::Justification::centred);
    }
}

void EnvelopeGraph::mouseDown (const juce::MouseEvent& e)
{
    auto area = getLocalBounds().toFloat().reduced (10.0f, 4.0f);
    activeStage_ = -1;
    float best = 14.0f;
    for (int i = 0; i < 4; ++i)
    {
        const auto d = e.position.getDistanceFrom (pointFor (i, area));
        if (d < best) { best = d; activeStage_ = i; }
    }
    repaint();
}

void EnvelopeGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (activeStage_ < 0) return;
    auto area = getLocalBounds().toFloat().reduced (10.0f, 4.0f);
    const auto index = static_cast<size_t> (activeStage_);
    const auto range = params_[index]->getNormalisableRange();
    float normalized = activeStage_ == 2
        ? 1.0f - (e.position.y - area.getY()) / area.getHeight()
        : (e.position.x - area.getX()) / area.getWidth();
    normalized = juce::jlimit (0.0f, 1.0f, normalized);
    sliders_[index].setValue (range.convertFrom0to1 (normalized), juce::sendNotificationSync);
    repaint();
}

void EnvelopeGraph::mouseUp (const juce::MouseEvent&)
{
    activeStage_ = -1;
    repaint();
}

void EnvelopeGraph::parameterValueChanged (int, float)
{
    triggerAsyncUpdate();
}

// ============================================================================
// RomplerEditor
// ============================================================================

RomplerEditor::RomplerEditor (RomplerProcessor& processorRef)
    : juce::AudioProcessorEditor (processorRef),
      processor_ (processorRef),
      presetLibrary_ (PresetStorage::userLibraryDirectory(), PresetStorage::factoryLibraryDirectory()),
      voiceBox_ ("VOICE"),
      busBox_ ("BUS"),
      compBox_ ("COMP"),
      envBox_ ("ENVELOPE"),
      fxBox_ ("FX")
{
    addAndMakeVisible (brandTitle_);
    brandTitle_.setText ("Aoi YUME", juce::dontSendNotification);
    brandTitle_.setJustificationType (juce::Justification::centredLeft);
    brandTitle_.setColour (juce::Label::textColourId, theme::knobCream);
    brandTitle_.setFont (makeFont (29.0f, true));

    // Keep the legacy child around for layout/accessibility compatibility, but
    // render the brand as a single paint-only engraving in the header below.
    // This prevents a second live label from sitting on top of the faceplate.
    addChildComponent (brandSub_);
    brandSub_.setText ("Aoi YUME", juce::dontSendNotification);
    brandSub_.setJustificationType (juce::Justification::centredLeft);
    brandSub_.setColour (juce::Label::textColourId, theme::mintGlow.withAlpha (0.92f));
    brandSub_.setFont (juce::Font (juce::FontOptions ("Snell Roundhand", 21.0f, juce::Font::italic)
                                       .withKerningFactor (0.02f)));

    addAndMakeVisible (presetName_);
    presetName_.setJustificationType (juce::Justification::centredLeft);
    presetName_.setColour (juce::Label::textColourId, theme::inkSoft);
    presetName_.setFont (makeDisplayFont (11.5f, true));
    addAndMakeVisible (presetDirtyIndicator_);
    presetDirtyIndicator_.setJustificationType (juce::Justification::centred);
    presetDirtyIndicator_.setColour (juce::Label::textColourId, theme::hot);
    presetDirtyIndicator_.setFont (makeDisplayFont (13.0f, true));
    presetName_.setText ("UNTITLED", juce::dontSendNotification);
    presetDirtyIndicator_.setText ("", juce::dontSendNotification);

    addAndMakeVisible (presetButton_);
    presetButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff071016));
    presetButton_.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff071016));
    presetButton_.onClick = [this] { openPresetBrowser(); };

    addAndMakeVisible (voiceBox_);
    addAndMakeVisible (busBox_);
    addAndMakeVisible (compBox_);
    addAndMakeVisible (envBox_);
    addAndMakeVisible (fxBox_);

    auto& apvts = processor_.getValueTreeState();
    controls_[0]  = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::voiceDrive)), true);
    controls_[1]  = std::make_unique<Switch> (*dynamic_cast<juce::AudioParameterChoice*>  (apvts.getParameter (ParamIDs::voiceCurve)));
    controls_[2]  = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::voiceVelToDrive)));
    controls_[3]  = std::make_unique<Toggle> (*dynamic_cast<juce::AudioParameterChoice*>  (apvts.getParameter (ParamIDs::voiceFilterRouting)));
    controls_[4]  = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::voiceFilterOffset)));
    controls_[5]  = std::make_unique<Stepper> (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::polyLimit)));
    controls_[6]  = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::busTapeDrive)), true);
    controls_[7]  = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::busFold)), true);
    controls_[29] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::busFilterCutoff)), true);
    controls_[30] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::busFilterResonance)), true);
    controls_[8]  = std::make_unique<Switch> (*dynamic_cast<juce::AudioParameterChoice*>  (apvts.getParameter (ParamIDs::busOsFactor)), juce::String(), true);
    controls_[9]  = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::outTrim)));
    controls_[10] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::outMix)));
    controls_[11] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxChorusRate)));
    controls_[12] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxChorusDepth)));
    controls_[13] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxChorusMix)));
    controls_[14] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxReverbRoom)));
    controls_[15] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxReverbDamp)));
    controls_[16] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxReverbMix)));
    controls_[27] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxDelayMix)), true);
    controls_[28] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxDelayFeedback)), true);
    controls_[17] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::envAttack)));
    controls_[18] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::envDecay)));
    controls_[19] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::envSustain)));
    controls_[20] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::envRelease)));
    controls_[21] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::compThreshold)));
    controls_[22] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::compRatio)));
    controls_[23] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::compAttack)));
    controls_[24] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::compRelease)));
    controls_[25] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::compMakeup)));
    controls_[26] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::compMix)));
    controls_[31] = std::make_unique<Toggle> (*dynamic_cast<juce::AudioParameterChoice*>  (apvts.getParameter (ParamIDs::voiceLegato)));
    envGraph_ = std::make_unique<EnvelopeGraph> (
        *dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::envAttack)),
        *dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::envDecay)),
        *dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::envSustain)),
        *dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::envRelease)));
    for (auto& c : controls_)
    {
        if (c)
            addAndMakeVisible (*c);
    }
    addAndMakeVisible (*envGraph_);

    addAndMakeVisible (compPathLabel_);
    compPathLabel_.setText ("POST BUS", juce::dontSendNotification);
    compPathLabel_.setJustificationType (juce::Justification::centredLeft);
    compPathLabel_.setColour (juce::Label::textColourId, theme::inkSoft);
    compPathLabel_.setFont (makeFont (8.8f, true));
    addAndMakeVisible (gainReductionMeter_);

    // UI labels follow the design mockup (shorter than the APVTS names).
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
    setKnobLabel (controls_[2], "VELOCITY > DRIVE");
    setChoiceLabel (controls_[3], "FILTER ROUTE");
    setKnobLabel (controls_[4], "FILTER OFFSET");
    setChoiceLabel (controls_[5], "POLYPHONY");
    setKnobLabel (controls_[6], "TAPE DRIVE");
    setKnobLabel (controls_[7], "FOLD");
    setKnobLabel (controls_[29], "FILTER CUTOFF");
    setKnobLabel (controls_[30], "RESONANCE");
    // Keep the 1x/2x/4x/8x selector visible without an extra "OVERSAMPLE"
    // caption; the segmented control is self-explanatory in the BUS strip.
    setChoiceLabel (controls_[8], "");
    setKnobLabel (controls_[9], "OUTPUT TRIM");
    setKnobLabel (controls_[10], "OUTPUT MIX");
    setKnobLabel (controls_[11], "CHORUS RATE");
    setKnobLabel (controls_[12], "CHORUS DEPTH");
    setKnobLabel (controls_[13], "CHORUS MIX");
    setKnobLabel (controls_[14], "REVERB ROOM");
    setKnobLabel (controls_[15], "REVERB DAMP");
    setKnobLabel (controls_[16], "REVERB MIX");
    setKnobLabel (controls_[17], "ATTACK");
    setKnobLabel (controls_[18], "DECAY");
    setKnobLabel (controls_[19], "SUSTAIN");
    setKnobLabel (controls_[20], "RELEASE");
    setKnobLabel (controls_[21], "THRESHOLD");
    setKnobLabel (controls_[22], "RATIO");
    setKnobLabel (controls_[23], "ATTACK");
    setKnobLabel (controls_[24], "RELEASE");
    setKnobLabel (controls_[25], "MAKEUP");
    setKnobLabel (controls_[26], "COMP MIX");
    setChoiceLabel (controls_[31], "LEGATO");

    // Dock: soundfont display + bank/program + load + peak meter.
    addAndMakeVisible (sfLabel_);
    sfLabel_.setText ("SOUNDFONT", juce::dontSendNotification);
    sfLabel_.setJustificationType (juce::Justification::centredLeft);
    sfLabel_.setColour (juce::Label::textColourId, theme::inkSoft);
    sfLabel_.setFont (makeFont (10.5f, true));

    addAndMakeVisible (sfDisplay_);
    sfDisplay_.setJustificationType (juce::Justification::centredLeft);
    sfDisplay_.setColour (juce::Label::textColourId, theme::displayOn);
    sfDisplay_.setColour (juce::Label::backgroundColourId, theme::displayBg);
    sfDisplay_.setFont (makeDisplayFont (11.8f, true));
    refreshDisplay();

    addAndMakeVisible (bankDigits_);
    bankDigits_.setJustificationType (juce::Justification::centred);
    bankDigits_.setColour (juce::Label::textColourId, theme::displayOn);
    bankDigits_.setColour (juce::Label::backgroundColourId, theme::displayBg);
    bankDigits_.setFont (makeDisplayFont (12.5f, true));

    addAndMakeVisible (loadButton_);
    loadButton_.setColour (juce::TextButton::buttonColourId, theme::mint);
    loadButton_.setColour (juce::TextButton::buttonOnColourId, theme::mintGlow);
    loadButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff071016));
    loadButton_.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff071016));
    loadButton_.onClick = [this] { onLoadButtonClicked(); };

    addAndMakeVisible (peakMeter_);

    // Bank/program browser: fills from the loaded SoundFont's preset table.
    bankBrowser_ = std::make_unique<BankBrowser>();
    bankBrowser_->setOnSelect ([this] (int bank, int program) {
        processor_.selectPreset (bank, program);
        for (auto& control : controls_)
            if (auto* knob = dynamic_cast<Knob*> (control.get()))
                knob->syncFromParameter();
    });
    addAndMakeVisible (*bankBrowser_);
    refreshPresetList();

    // The library browser is an opaque, non-modal child.  It starts hidden so
    // the existing instrument layout remains the default host view.
    presetOverlay_ = std::make_unique<PresetBrowserOverlay> (presetLibrary_);
    presetOverlay_->onLoadRequested = [this] (juce::String uuid) { loadPresetUuid (uuid); };
    presetOverlay_->onDirtyChoice = [this] (juce::String uuid, PresetBrowserOverlay::DirtyChoice choice)
    {
        handlePresetDirtyChoice (uuid, choice);
    };
    presetOverlay_->onCloseRequested = [this] { closePresetBrowser(); };
    presetLibrary_.onCatalogueChanged = [this]
    {
        if (presetOverlay_)
            presetOverlay_->refresh();
    };
    addAndMakeVisible (*presetOverlay_);
    presetOverlay_->setVisible (false);

    addAndMakeVisible (keyboard_);
    keyboard_.setKeyRange (36, 36);   // C2..B4
    keyboard_.setNoteCallback ([this] (int note, bool on) {
        processor_.postNote (note, on, 100);
    });

    // Pitch / mod wheels sit beside the keyboard, like a real synth's
    // keybed. Dragging posts through the processor's atomics so behaviour
    // matches live MIDI exactly; the 20 Hz timer below pulls the processor's
    // current value back into the wheel so real MIDI/host automation moves
    // the widget too, even though the drag itself never touches it.
    pitchWheel_ = std::make_unique<PitchWheel> ([this] (float v) { processor_.postPitchWheel (v); });
    modWheel_   = std::make_unique<ModWheel>   ([this] (float v) { processor_.postModWheel (v); });
    addAndMakeVisible (*pitchWheel_);
    addAndMakeVisible (*modWheel_);

    // Landscape synth-deck layout: the preset browser stays visible beside
    // the primary voice controls, while the performance keyboard anchors the
    // bottom edge.
    setSize (1120, 900);

    startTimerHz (20);
    activePreset_ = processor_.capturePreset();
    hasActivePreset_ = true;
    refreshPresetHeader();
}

RomplerEditor::~RomplerEditor() = default;

void RomplerEditor::setPresetHeaderDocument (const PresetDocument& document)
{
    activePreset_ = document;
    hasActivePreset_ = true;
    presetDirty_ = false;
    refreshPresetHeader();
}

void RomplerEditor::markPresetHeaderSaved()
{
    if (! hasActivePreset_)
        return;
    const auto name = activePreset_.name;
    activePreset_ = processor_.capturePreset();
    activePreset_.name = name;
    presetDirty_ = false;
    refreshPresetHeader();
}

void RomplerEditor::refreshPresetHeader()
{
    if (! hasActivePreset_)
        return;
    const auto name = activePreset_.name.isEmpty() ? juce::String ("UNTITLED") : activePreset_.name;
    presetName_.setText (name, juce::dontSendNotification);
    presetDirtyIndicator_.setText (presetDirty_ ? "*" : "", juce::dontSendNotification);
}

void RomplerEditor::openPresetBrowser()
{
    if (! presetOverlay_)
        return;

    presetLibrary_.rescan();
    presetOverlay_->setDirty (presetDirty_);
    presetOverlay_->setBounds (getLocalBounds());
    presetOverlay_->setVisible (true);
    presetOverlay_->toFront (false);
    // EDITOR_WANTS_KEYBOARD_FOCUS is deliberately false.  Do not call
    // grabKeyboardFocus here; mouse and host focus ownership remain intact.
}

void RomplerEditor::closePresetBrowser()
{
    if (presetOverlay_)
        presetOverlay_->setVisible (false);
}

void RomplerEditor::loadPresetUuid (const juce::String& uuid)
{
    const auto* entry = presetLibrary_.findByUuid (uuid);
    if (entry == nullptr)
        return;

    if (processor_.applyPreset (entry->document) != RomplerProcessor::ApplyStatus::ok)
        return;

    setPresetHeaderDocument (entry->document);
    for (auto& control : controls_)
        if (auto* knob = dynamic_cast<Knob*> (control.get()))
            knob->syncFromParameter();
    refreshDisplay();
    refreshPresetList();
    if (presetOverlay_)
        presetOverlay_->setCurrentUuid (uuid);
    closePresetBrowser();
}

void RomplerEditor::handlePresetDirtyChoice (const juce::String& uuid,
                                             PresetBrowserOverlay::DirtyChoice choice)
{
    if (choice == PresetBrowserOverlay::DirtyChoice::cancel)
        return;

    if (choice == PresetBrowserOverlay::DirtyChoice::save)
    {
        // Save the current audible state as a user preset before switching.
        // The library deliberately assigns a unique name when the current
        // document has no stable user-file identity.
        juce::String savedUuid;
        const auto name = activePreset_.name.isEmpty() ? juce::String ("UNTITLED") : activePreset_.name;
        if (presetLibrary_.saveAs (processor_.capturePreset(), name, savedUuid) != LibraryStatus::ok)
            return;
        markPresetHeaderSaved();
    }

    loadPresetUuid (uuid);
}

void RomplerEditor::paint (juce::Graphics& g)
{
    // Main moulded chassis: three depth planes make the editor read as an
    // instrument faceplate, while keeping components and signal behaviour
    // exactly as before.
    g.fillAll (juce::Colour (0xff050808));
    const auto chassis = getLocalBounds().toFloat().reduced (6.0f);
    g.setColour (juce::Colour (0xc4000000));
    g.fillRoundedRectangle (chassis.translated (0.0f, 4.0f), 17.0f);

    juce::ColourGradient outerGrad (juce::Colour (0xff8b998e), chassis.getTopLeft(),
                                    juce::Colour (0xff0c1211), chassis.getBottomLeft(), false);
    g.setGradientFill (outerGrad);
    g.fillRoundedRectangle (chassis, 17.0f);
    g.setColour (juce::Colour (0x64fffff5));
    g.drawRoundedRectangle (chassis.reduced (0.55f), 16.4f, 1.05f);

    const auto face = chassis.reduced (3.0f);
    juce::ColourGradient bodyGrad (juce::Colour (0xff4a5750), face.getTopLeft(),
                                   juce::Colour (0xff1b2421), face.getBottomLeft(), false);
    g.setGradientFill (bodyGrad);
    g.fillRoundedRectangle (face, 13.5f);
    g.setColour (juce::Colour (0x3dffffff));
    g.drawRoundedRectangle (face.reduced (0.65f), 12.8f, 0.75f);
    g.setColour (juce::Colour (0x75000000));
    g.drawRoundedRectangle (face.reduced (1.5f).translated (0.0f, 0.8f), 12.0f, 1.0f);
    addBrushedGrain (g, face.reduced (2.0f), 7);

    // Narrow metal cheeks frame the whole instrument and make the virtual
    // panel feel like a fabricated enclosure rather than a browser surface.
    const auto leftRail = juce::Rectangle<float> (face.getX() + 5.0f, face.getY() + 66.0f,
                                                   7.5f, face.getHeight() - 78.0f);
    const auto rightRail = juce::Rectangle<float> (face.getRight() - 12.5f, face.getY() + 66.0f,
                                                    7.5f, face.getHeight() - 78.0f);
    drawChassisRail (g, leftRail, 41);
    drawChassisRail (g, rightRail, 73);

    // A separate raised header gives the typography a believable physical
    // home instead of treating it as screen chrome.
    const auto topPanel = face.withHeight (58.0f);
    g.setColour (juce::Colour (0x8e000000));
    g.fillRoundedRectangle (topPanel.translated (0.0f, 1.8f), 10.0f);
    juce::ColourGradient headerGrad (juce::Colour (0xff59665d), topPanel.getTopLeft(),
                                     juce::Colour (0xff1b2421), topPanel.getBottomLeft(), false);
    g.setGradientFill (headerGrad);
    g.fillRoundedRectangle (topPanel, 10.0f);
    g.setColour (juce::Colour (0x56ffffff));
    g.drawRoundedRectangle (topPanel.reduced (0.55f), 9.4f, 0.75f);
    g.setColour (theme::mintDeep.withAlpha (0.74f));
    g.fillRoundedRectangle (topPanel.withTrimmedLeft (12.0f).withTrimmedRight (12.0f)
                                     .withHeight (1.35f).translated (0.0f, 1.2f), 0.65f);
    g.setColour (juce::Colour (0x70000000));
    g.fillRect (topPanel.withY (topPanel.getBottom() - 2.0f).withHeight (2.0f));
    addBrushedGrain (g, topPanel.reduced (2.0f), 19);

    // One restrained, uppercase signature engraving on the metal header:
    // the lower dark pass sits in the cut, while the tiny upper highlight
    // catches the faceplate edge without reading as a glowing UI label.
    const auto signature = topPanel.withTrimmedLeft (238.0f)
                                    .withTrimmedRight (420.0f)
                                    .withY (topPanel.getY() + 14.0f)
                                    .withHeight (30.0f);
    const auto signatureFont = juce::Font (juce::FontOptions ("Snell Roundhand", 21.0f,
                                                               juce::Font::italic)
                                               .withKerningFactor (0.02f));
    g.setFont (signatureFont);
    g.setColour (juce::Colour (0xa6000000));
    g.drawFittedText ("AOI YUME", signature.translated (0.0f, 1.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.9f);
    g.setColour (juce::Colour (0x45ffffff));
    g.drawFittedText ("AOI YUME", signature.translated (0.0f, -0.25f).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.9f);

    // Frame the two readouts as smoky LCDs. The child Labels retain their
    // original interaction/accessibility, while the surround supplies depth.
    const auto drawLcdFrame = [&g] (juce::Rectangle<int> bounds)
    {
        if (bounds.isEmpty())
            return;
        const auto outer = bounds.toFloat().expanded (3.0f, 2.5f);
        g.setColour (juce::Colour (0x9e000000));
        g.fillRoundedRectangle (outer.translated (0.0f, 1.3f), 4.2f);
        juce::ColourGradient rim (juce::Colour (0xff6b7a70), outer.getTopLeft(),
                                  juce::Colour (0xff0c1412), outer.getBottomLeft(), false);
        g.setGradientFill (rim);
        g.fillRoundedRectangle (outer, 4.2f);
        g.setColour (theme::displayOn.withAlpha (0.34f));
        g.drawRoundedRectangle (outer.reduced (1.55f), 2.9f, 0.65f);
        g.setColour (juce::Colour (0x1affffff));
        g.drawLine (outer.getX() + 4.0f, outer.getY() + 3.0f,
                    outer.getRight() - 9.0f, outer.getY() + 3.0f, 0.62f);
    };
    drawLcdFrame (sfDisplay_.getBounds());
    drawLcdFrame (bankDigits_.getBounds());

    drawMachineScrew (g, { face.getRight() - 11.0f, face.getY() + 11.0f }, 2.45f);
    drawMachineScrew (g, { face.getX() + 11.0f, face.getBottom() - 11.0f }, 2.45f);
    drawMachineScrew (g, { face.getRight() - 11.0f, face.getBottom() - 11.0f }, 2.45f);
}

void RomplerEditor::resized()
{
    auto b = getLocalBounds().reduced (14);

    auto top = b.removeFromTop (58);
    const auto brandColumn = top.removeFromLeft (220);
    brandTitle_.setBounds (brandColumn.withTop (top.getY() + 1).withHeight (30));
    brandSub_.setBounds (brandColumn.withTop (top.getY() + 29).withHeight (25));
    presetName_.setBounds (brandColumn.withTop (top.getY() + 32).withHeight (20).withTrimmedRight (24));
    presetDirtyIndicator_.setBounds (brandColumn.getRight() - 24, top.getY() + 32, 24, 20);
    auto presetButtonArea = top.removeFromRight (100);
    presetButton_.setBounds (presetButtonArea.withY (top.getY() + 14).withHeight (30));
    // The browser is intentionally the first-class element: selecting a
    // patch and seeing its bank/program is the main entry point of a rompler.
    constexpr int browserWidth = 318;
    constexpr int topHeight = 210;
    auto topRow = b.removeFromTop (topHeight);
    auto browser = topRow.removeFromLeft (browserWidth);
    bankBrowser_->setBounds (browser);
    topRow.removeFromLeft (12);
    voiceBox_.setBounds (topRow);
    layoutVoiceControls (voiceBox_.getBounds().reduced (10, 12).withTop (voiceBox_.getY() + 24));

    b.removeFromTop (10);

    // The audio path reads left-to-right: BUS -> COMP -> ENVELOPE. The
    // compressor gets a compact but complete 3 x 2 control matrix.
    constexpr int busWidth = 345;
    constexpr int compWidth = 330;
    constexpr int midHeight = 225;
    auto midRow = b.removeFromTop (midHeight);
    busBox_.setBounds (midRow.removeFromLeft (busWidth));
    midRow.removeFromLeft (10);
    compBox_.setBounds (midRow.removeFromLeft (compWidth));
    midRow.removeFromLeft (10);
    envBox_.setBounds (midRow);
    layoutBusControls (busBox_.getBounds().reduced (10, 12).withTop (busBox_.getY() + 24));
    layoutCompControls (compBox_.getBounds().reduced (10, 12).withTop (compBox_.getY() + 24));
    compPathLabel_.setBounds (compBox_.getX() + 102, compBox_.getY() + 4, 64, 16);
    gainReductionMeter_.setBounds (compBox_.getRight() - 82, compBox_.getY() + 5, 70, 15);
    auto envArea = envBox_.getBounds().reduced (10, 12).withTop (envBox_.getY() + 24);
    envGraph_->setBounds (envArea.withHeight (76));
    layoutEnvControls (envArea.withY (envArea.getY() + 82).withHeight (envArea.getHeight() - 82));

    b.removeFromTop (10);

    // FX box spans the full width.
    constexpr int fxHeight = 140;
    auto fx = b.removeFromTop (fxHeight);
    fxBox_.setBounds (fx);
    layoutFxControls (fxBox_.getBounds().reduced (10, 12).withTop (fxBox_.getY() + 24));

    b.removeFromTop (10);

    // Dock.
    auto dock = b.removeFromTop (48);
    const int dx = dock.getX();
    const int dy = dock.getY();
    const int loadX  = dock.getRight() - 96;
    const int bankX  = loadX - 8 - 76;
    const int meterX = bankX - 12 - 88;
    sfLabel_.setBounds   (dx,       dy + 1,  92,                  14);
    sfDisplay_.setBounds (dx + 97,  dy + 18, meterX - dx - 97 - 12, 24);
    peakMeter_.setBounds (meterX,   dy + 20, 88,                  20);
    bankDigits_.setBounds (bankX,   dy + 18, 76,                  24);
    loadButton_.setBounds (loadX,   dy + 6, 96,                   36);

    b.removeFromTop (8);

    // Pitch / mod wheels flank the keyboard's left edge, hardware-style.
    auto wheelColumn = b.removeFromLeft (100);
    const int wheelH = juce::jmin (168, wheelColumn.getHeight());
    const int wheelY = wheelColumn.getY() + (wheelColumn.getHeight() - wheelH) / 2;
    if (pitchWheel_)
        pitchWheel_->setBounds (wheelColumn.removeFromLeft (46).withY (wheelY).withHeight (wheelH));
    if (modWheel_)
        modWheel_->setBounds (wheelColumn.removeFromLeft (46).withY (wheelY).withHeight (wheelH));
    b.removeFromLeft (8);

    keyboard_.setBounds (b);
}

void RomplerEditor::layoutVoiceControls (juce::Rectangle<int> area)
{
    // 4x2 grid: Drive | Vel->Drive | Filter Offset | Legato / Curve | Filter
    // Route | Polyphony | (empty).
    const auto rows = 2;
    const auto cols = 4;
    const auto cellW = area.getWidth() / cols;
    const auto cellH = area.getHeight() / rows;

    auto place = [&] (int idx, int r, int c) {
        if (idx >= 0 && controls_[static_cast<size_t> (idx)])
            controls_[static_cast<size_t> (idx)]->setBounds (
                area.withX (area.getX() + c * cellW).withY (area.getY() + r * cellH)
                    .withSize (cellW, cellH).reduced (4, 2));
    };
    place (0, 0, 0);   // Drive
    place (2, 0, 1);   // Vel -> Drive
    place (4, 0, 2);   // Filter Offset
    place (31, 0, 3);  // Legato
    place (1, 1, 0);   // Curve
    place (3, 1, 1);   // Filter Route
    place (5, 1, 2);   // Polyphony
}

void RomplerEditor::layoutBusControls (juce::Rectangle<int> area)
{
    const auto rows = 2;
    const auto cols = 4;
    const auto cellW = area.getWidth() / cols;
    const auto cellH = area.getHeight() / rows;

    auto place = [&] (int idx, int r, int c) {
        if (idx >= 0 && controls_[static_cast<size_t> (idx)])
            controls_[static_cast<size_t> (idx)]->setBounds (
                area.withX (area.getX() + c * cellW).withY (area.getY() + r * cellH)
                    .withSize (cellW, cellH).reduced (4, 2));
    };
    place (6, 0, 0);   // Tape Drive
    place (7, 0, 1);   // Fold
    place (29, 0, 2);  // Filter cutoff
    place (30, 0, 3);  // Filter resonance
    place (8, 1, 0);   // Oversampling factor
    place (9, 1, 1);   // Out Trim
    place (10, 1, 2);  // Mix
}

void RomplerEditor::layoutCompControls (juce::Rectangle<int> area)
{
    constexpr int rows = 2;
    constexpr int cols = 3;
    const int cellW = area.getWidth() / cols;
    const int cellH = area.getHeight() / rows;

    auto place = [&] (int idx, int row, int column) {
        if (controls_[static_cast<size_t> (idx)])
            controls_[static_cast<size_t> (idx)]->setBounds (
                area.withX (area.getX() + column * cellW).withY (area.getY() + row * cellH)
                    .withSize (cellW, cellH).reduced (4, 2));
    };

    place (21, 0, 0);  // Threshold
    place (22, 0, 1);  // Ratio
    place (23, 0, 2);  // Attack
    place (24, 1, 0);  // Release
    place (25, 1, 1);  // Makeup
    place (26, 1, 2);  // Wet/dry mix
}

void RomplerEditor::layoutEnvControls (juce::Rectangle<int> area)
{
    const auto count = 4;
    const auto cellW = area.getWidth() / count;
    for (int i = 0; i < count; ++i)
    {
        const int idx = 17 + i;
        if (controls_[static_cast<std::size_t> (idx)])
            controls_[static_cast<std::size_t> (idx)]->setBounds (
                area.withX (area.getX() + i * cellW).withSize (cellW, area.getHeight()).reduced (6, 2));
    }
}

void RomplerEditor::layoutFxControls (juce::Rectangle<int> area)
{
    const auto count = 8;
    const auto cellW = area.getWidth() / count;
    for (int i = 0; i < count; ++i)
    {
        const int idx = i < 6 ? 11 + i : 27 + (i - 6);
        if (controls_[static_cast<size_t> (idx)])
            controls_[static_cast<size_t> (idx)]->setBounds (
                area.withX (area.getX() + i * cellW).withY (area.getY())
                    .withSize (cellW, area.getHeight()).reduced (6, 2));
    }
}

void RomplerEditor::refreshDisplay()
{
    const auto file = processor_.getLoadedFileName();
    juce::String text = file.isEmpty() ? "NO FILE LOADED" : file;
    sfDisplay_.setText (text, juce::dontSendNotification);
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
}

void RomplerEditor::onLoadButtonClicked()
{
    fileChooser_ = std::make_unique<juce::FileChooser> (
        "Load a SoundFont...", processor_.getBundledSoundFontsDirectory(), "*.sf2;*.sf3");
    fileChooser_->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
            {
                processor_.loadSoundFont (file);
                refreshPresetList();
            }
            fileChooser_.reset();
        });
}

void RomplerEditor::comboBoxChanged (juce::ComboBox*)
{
    // No preset combo in this layout; nothing to do.
}

void RomplerEditor::timerCallback()
{
    peakMeter_.setLevel (processor_.getLastPeak());
    gainReductionMeter_.setReductionDb (processor_.getLastCompressionReductionDb());

    const auto [bank, program] = processor_.getCurrentBankProgram();
    bankDigits_.setText (juce::String (bank).paddedLeft ('0', 2), juce::dontSendNotification);

    if (bankBrowser_)
        bankBrowser_->setCurrent (bank, program);

    if (presetOverlay_ && presetOverlay_->isVisible())
        presetOverlay_->setBounds (getLocalBounds());

    // Reflect live MIDI (or host automation) into the wheel widgets. Both
    // setValue() calls are no-ops while the user is actively dragging that
    // wheel, so this never fights the mouse.
    if (pitchWheel_)
        pitchWheel_->setValue (processor_.getPitchWheelNormalized());
    if (modWheel_)
        modWheel_->setValue (processor_.getModWheelNormalized());

    if (hasActivePreset_)
    {
        const bool nowDirty = ! presetStateEqual (processor_.capturePreset(), activePreset_);
        if (nowDirty != presetDirty_)
        {
            presetDirty_ = nowDirty;
            refreshPresetHeader();
        }
    }
}

} // namespace aod
