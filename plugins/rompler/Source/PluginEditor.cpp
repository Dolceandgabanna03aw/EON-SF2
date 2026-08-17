#include "PluginEditor.h"

namespace aod
{

// ============================================================================
// SectionBox
// ============================================================================

SectionBox::SectionBox (const juce::String& title)
{
    addAndMakeVisible (title_);
    title_.setText (title, juce::dontSendNotification);
    title_.setJustificationType (juce::Justification::centred);
    title_.setColour (juce::Label::textColourId, juce::Colour (0xff06231b));
    title_.setFont (makeFont (10.0f, true));
    title_.setBounds (10, -8, 80, 16);
}

SectionBox::~SectionBox() = default;

void SectionBox::paint (juce::Graphics& g)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (theme::mintDeep);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.5f);

    // The tab: a mint rectangle sitting on the top edge behind the title.
    const auto tab = juce::Rectangle<int> (10, -8, 80, 16);
    g.setColour (theme::mintDeep);
    g.fillRoundedRectangle (tab.toFloat(), 3.0f);
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

    addAndMakeVisible (name_);
    name_.setText (param.getName (32), juce::dontSendNotification);
    name_.setJustificationType (juce::Justification::centred);
    name_.setColour (juce::Label::textColourId, theme::ink);
    name_.setFont (makeFont (9.5f, true));

    addAndMakeVisible (value_);
    value_.setJustificationType (juce::Justification::centred);
    value_.setColour (juce::Label::textColourId, theme::inkSoft);
    value_.setFont (makeFont (9.0f, false));

    setSize (70, 96);
}

Knob::~Knob() = default;

void Knob::paint (juce::Graphics& g)
{
    const float w = (float) getWidth();
    const float h = (float) getHeight();
    const float knobSize = std::min (std::min (w, h - 30.0f), 56.0f);
    auto circle = juce::Rectangle<float> (0.0f, 0.0f, w, h)
                      .withSizeKeepingCentre (knobSize, knobSize);

    // Knob cap: cream (or hot orange for the drive-shaping controls).
    juce::ColourGradient grad (hot_ ? juce::Colour (0xffffd9b8) : juce::Colour (0xffffffff),
                               circle.getTopLeft(),
                               hot_ ? theme::hotDeep : theme::knobShadow,
                               circle.getBottomLeft(), false);
    g.setGradientFill (grad);
    g.fillEllipse (circle);
    g.setColour (juce::Colour (0x33000000));
    g.drawEllipse (circle, 1.0f);

    // Pointer (mint on cream knobs, dark on hot knobs).
    const float unit = static_cast<float> (slider_.getValue());
    const float angleRad = (juce::MathConstants<float>::pi / 180.0f) * ((unit * 264.0f) - 132.0f);
    const auto centre = circle.getCentre();
    const float r = circle.getWidth() * 0.5f - 5.0f;
    const juce::Point<float> tip (centre.x + r * std::cos (angleRad),
                                  centre.y + r * std::sin (angleRad));
    g.setColour (hot_ ? juce::Colour (0xff4a2408) : theme::mintDeep);
    g.drawLine (centre.x, centre.y, tip.x, tip.y, 2.5f);

    value_.setText (slider_.getTextFromValue (slider_.getValue()), juce::dontSendNotification);
}

void Knob::resized()
{
    const auto w = getWidth();
    const auto h = getHeight();
    name_.setBounds (getLocalBounds().withSizeKeepingCentre (w, 14).withY (0));
    value_.setBounds (getLocalBounds().withSizeKeepingCentre (w, 12).withY (h - 12));
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    lastDragY_ = e.y;
    setMouseCursor (juce::MouseCursor::IBeamCursor);
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    const float delta = (lastDragY_ - e.y) / 60.0f;
    lastDragY_ = e.y;
    slider_.setValue (slider_.getValue() + delta, juce::sendNotificationSync);
}

void Knob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const auto range = param_.getNormalisableRange();
    const float step = (range.end - range.start) / 100.0f;
    slider_.setValue (slider_.getValue() + (float) wheel.deltaY * step, juce::sendNotificationSync);
}

// ============================================================================
// Switch (segmented buttons)
// ============================================================================

Switch::Switch (juce::AudioParameterChoice& param)
    : param_ (param)
{
    addAndMakeVisible (label_);
    label_.setText (param.getName (32), juce::dontSendNotification);
    label_.setJustificationType (juce::Justification::centred);
    label_.setColour (juce::Label::textColourId, theme::ink);
    label_.setFont (makeFont (9.5f, true));

    for (const auto& choice : param.getAllValueStrings())
        box_.addItem (choice, box_.getNumItems() + 1);
    attachment_ = std::make_unique<juce::ComboBoxParameterAttachment> (param, box_);
    rebuildSegments();

    setSize (150, 70);
}

Switch::~Switch() = default;

void Switch::rebuildSegments()
{
    segments_.clear();
    numSegments_ = box_.getNumItems();
    if (numSegments_ == 0)
        return;
    const float w = (float) getWidth() - 16.0f;
    const float segW = w / (float) numSegments_;
    for (int i = 0; i < numSegments_; ++i)
        segments_.emplace_back (8 + (int) (i * segW), 22, (int) segW, 22);
}

void Switch::resized()
{
    const auto w = getWidth();
    label_.setBounds (getLocalBounds().withSizeKeepingCentre (w, 14).withY (0));
    rebuildSegments();
}

void Switch::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0x1f000000));
    g.fillRoundedRectangle (b.withTrimmedLeft (8).withTrimmedRight (8).withTop (22).withHeight (22), 4.0f);

    for (int i = 0; i < numSegments_; ++i)
    {
        auto seg = segments_[static_cast<std::size_t> (i)].toFloat().reduced (1.5f);
        const bool active = (box_.getSelectedItemIndex() == i);
        if (active)
        {
            g.setGradientFill (juce::ColourGradient (theme::mint, seg.getTopLeft(),
                                                     theme::mintDeep, seg.getBottomLeft(), false));
            g.fillRoundedRectangle (seg, 3.0f);
            g.setColour (juce::Colour (0xff06231b));
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2ecd8), seg.getTopLeft(),
                                                     juce::Colour (0xffd3cbac), seg.getBottomLeft(), false));
            g.fillRoundedRectangle (seg, 3.0f);
            g.setColour (theme::inkSoft);
        }
        g.drawText (box_.getItemText (i + 1), seg, juce::Justification::centred);
    }
}

void Switch::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < numSegments_; ++i)
        if (segments_[static_cast<std::size_t> (i)].contains (e.getPosition()))
        {
            box_.setSelectedId (i + 1, juce::sendNotificationSync);
            repaint();
            break;
        }
}

// ============================================================================
// Toggle (2-way)
// ============================================================================

Toggle::Toggle (juce::AudioParameterChoice& param)
    : param_ (param)
{
    addAndMakeVisible (label_);
    label_.setText (param.getName (32), juce::dontSendNotification);
    label_.setJustificationType (juce::Justification::centred);
    label_.setColour (juce::Label::textColourId, theme::ink);
    label_.setFont (makeFont (9.5f, true));

    for (const auto& choice : param.getAllValueStrings())
        box_.addItem (choice, box_.getNumItems() + 1);
    attachment_ = std::make_unique<juce::ComboBoxParameterAttachment> (param, box_);

    setSize (110, 70);
}

Toggle::~Toggle() = default;

void Toggle::resized()
{
    const auto w = getWidth();
    label_.setBounds (getLocalBounds().withSizeKeepingCentre (w, 14).withY (0));
}

void Toggle::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const int idx = box_.getSelectedItemIndex();

    const float trackW = 44.0f;
    const float trackH = 20.0f;
    auto track = juce::Rectangle<float> ((b.getWidth() - trackW) * 0.5f, 24.0f, trackW, trackH);

    const bool on = (idx == 1);
    g.setColour (on ? theme::mintDeep : juce::Colour (0xffa49c7e));
    g.fillRoundedRectangle (track, trackH * 0.5f);
    g.setColour (juce::Colour (0x40000000));
    g.drawRoundedRectangle (track, trackH * 0.5f, 1.0f);

    const float thumbD = 14.0f;
    const float thumbX = on ? track.getX() + track.getWidth() - thumbD - 2.0f : track.getX() + 2.0f;
    g.setColour (juce::Colour (0xfff2ecd8));
    g.fillEllipse (thumbX, track.getY() + 2.0f, thumbD, thumbD);

    // Pre / Post labels either side.
    g.setColour (on ? theme::mintDeep : theme::inkSoft);
    g.setFont (makeFont (8.0f, true));
    g.drawText ("PRE",  juce::roundToInt (track.getX()) - 30, 26, 26, 16, juce::Justification::centred);
    g.drawText ("POST", juce::roundToInt (track.getRight()) + 4, 26, 30, 16, juce::Justification::centred);
}

void Toggle::mouseDown (const juce::MouseEvent&)
{
    box_.setSelectedId ((box_.getSelectedId() == 1) ? 2 : 1, juce::sendNotificationSync);
    repaint();
}

// ============================================================================
// Stepper (polyphony)
// ============================================================================

Stepper::Stepper (juce::RangedAudioParameter& param)
    : param_ (param)
{
    addAndMakeVisible (label_);
    label_.setText (param.getName (32), juce::dontSendNotification);
    label_.setJustificationType (juce::Justification::centred);
    label_.setColour (juce::Label::textColourId, theme::ink);
    label_.setFont (makeFont (9.5f, true));

    slider_.setRange (param.getNormalisableRange().start, param.getNormalisableRange().end, 1.0);
    slider_.setValue (param.getValue(), juce::dontSendNotification);
    attachment_ = std::make_unique<juce::SliderParameterAttachment> (param, slider_);

    setSize (110, 70);
}

Stepper::~Stepper() = default;

void Stepper::resized()
{
    const auto w = getWidth();
    label_.setBounds (getLocalBounds().withSizeKeepingCentre (w, 14).withY (0));
}

void Stepper::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const int value = juce::roundToInt (slider_.getValue());

    auto digits = juce::Rectangle<float> ((b.getWidth() - 78.0f) * 0.5f, 22.0f, 46.0f, 28.0f);
    g.setColour (theme::displayBg);
    g.fillRoundedRectangle (digits, 3.0f);
    g.setColour (theme::mint);
    g.setFont (makeFont (15.0f, true));
    g.drawText (juce::String (value).paddedLeft ('0', 2), digits, juce::Justification::centred);

    // Up / down arrow buttons.
    auto arrows = juce::Rectangle<float> (digits.getRight() + 6.0f, digits.getY(), 16.0f, 28.0f);
    const float ah = arrows.getHeight() * 0.5f - 1.0f;
    for (int i = 0; i < 2; ++i)
    {
        auto btn = juce::Rectangle<float> (arrows.getX(), arrows.getY() + i * (ah + 2.0f), 16.0f, ah);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2ecd8), btn.getTopLeft(),
                                                 juce::Colour (0xffd3cbac), btn.getBottomLeft(), false));
        g.fillRoundedRectangle (btn, 2.0f);
        g.setColour (theme::ink);
        g.setFont (makeFont (8.0f, true));
        g.drawText (i == 0 ? "\u25b2" : "\u25bc", btn, juce::Justification::centred);
    }
    upArrow_ = arrows.toNearestInt().withHeight (juce::roundToInt (ah)).withY (juce::roundToInt (arrows.getY()));
    downArrow_ = arrows.toNearestInt().withY (juce::roundToInt (arrows.getY()) + juce::roundToInt (ah + 2.0f)).withHeight (juce::roundToInt (ah));
}

void Stepper::mouseDown (const juce::MouseEvent& e)
{
    if (upArrow_.contains (e.getPosition()))
        slider_.setValue (slider_.getValue() + 1.0, juce::sendNotificationSync);
    else if (downArrow_.contains (e.getPosition()))
        slider_.setValue (slider_.getValue() - 1.0, juce::sendNotificationSync);
    repaint();
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
    const float w = (float) getWidth();
    const float h = (float) getHeight();
    const float gap = 3.0f;
    const float ledW = (w - gap * (numSegments - 1)) / (float) numSegments;
    const int lit = juce::roundToInt (level_ * (float) numSegments);

    for (int i = 0; i < numSegments; ++i)
    {
        auto r = juce::Rectangle<float> (i * (ledW + gap), 0.0f, ledW, h);
        const bool on = i < lit;
        const auto colour = (i >= numSegments - 1) ? theme::ledRed
                          : (i >= numSegments - 3) ? theme::ledHot
                                                   : theme::ledMint;
        g.setColour (on ? colour : theme::ledOff);
        g.fillRoundedRectangle (r, 2.0f);
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

    const int blackAfter[7] = { 1, 1, 0, 1, 1, 1, 0 };   // after C D E F G A B
    for (int i = 0; i < numKeys; ++i)
    {
        const int note = lowNote + i;
        const int pc = note % 7;
        if (blackAfter[pc] == 1)
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
    g.fillAll (theme::body2);

    for (const auto& k : keys_)
        if (!k.black)
        {
            auto r = boundsFor (k).toFloat();
            const bool lit = lit_[k.note];
            juce::Colour top = lit ? theme::mint : theme::knobCream;
            juce::Colour bot = lit ? theme::mintDeep : theme::knobShadow;
            g.setGradientFill (juce::ColourGradient (top, r.getTopLeft(), bot, r.getBottomLeft(), false));
            g.fillRoundedRectangle (r, 2.0f);
            g.setColour (juce::Colour (0xff7d786d));
            g.drawRoundedRectangle (r, 2.0f, 1.0f);
        }

    for (const auto& k : keys_)
        if (k.black)
        {
            auto r = boundsFor (k).toFloat();
            const bool lit = lit_[k.note];
            juce::Colour top = lit ? theme::mint : juce::Colour (0xff4a4a4a);
            juce::Colour bot = lit ? theme::mintDeep : juce::Colour (0xff0d0d0d);
            g.setGradientFill (juce::ColourGradient (top, r.getTopLeft(), bot, r.getBottomLeft(), false));
            g.fillRoundedRectangle (r, 2.0f);
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
// RomplerEditor
// ============================================================================

RomplerEditor::RomplerEditor (RomplerProcessor& processorRef)
    : juce::AudioProcessorEditor (processorRef),
      processor_ (processorRef),
      voiceBox_ ("VOICE"),
      busBox_ ("BUS"),
      fxBox_ ("FX")
{
    addAndMakeVisible (brandTitle_);
    brandTitle_.setText ("EON-DS50", juce::dontSendNotification);
    brandTitle_.setJustificationType (juce::Justification::centredLeft);
    brandTitle_.setColour (juce::Label::textColourId, theme::mint);
    brandTitle_.setFont (makeFont (26.0f, true));

    addAndMakeVisible (brandSub_);
    brandSub_.setText ("SF2 ROMPLER  \u00b7  NONLINEAR DRIVE", juce::dontSendNotification);
    brandSub_.setJustificationType (juce::Justification::centredLeft);
    brandSub_.setColour (juce::Label::textColourId, theme::mint);
    brandSub_.setFont (makeFont (10.0f, false));

    addAndMakeVisible (badges_);
    badges_.setJustificationType (juce::Justification::centredRight);
    badges_.setColour (juce::Label::textColourId, juce::Colour (0xff8b9186));
    badges_.setFont (makeFont (9.5f, false));
    badges_.setText ("VST3 \u00b7 AU \u00b7 STANDALONE\nSisyphus Audio \u2014 M1", juce::dontSendNotification);

    addAndMakeVisible (voiceBox_);
    addAndMakeVisible (busBox_);
    addAndMakeVisible (fxBox_);

    auto& apvts = processor_.getValueTreeState();
    controls_[0]  = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::voiceDrive)), true);
    controls_[1]  = std::make_unique<Switch> (*dynamic_cast<juce::AudioParameterChoice*>  (apvts.getParameter (ParamIDs::voiceCurve)));
    controls_[2]  = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::voiceVelToDrive)), true);
    controls_[3]  = std::make_unique<Toggle> (*dynamic_cast<juce::AudioParameterChoice*>  (apvts.getParameter (ParamIDs::voiceFilterRouting)));
    controls_[4]  = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::voiceFilterOffset)));
    controls_[5]  = std::make_unique<Stepper> (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::polyLimit)));
    controls_[6]  = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::busTapeDrive)), true);
    controls_[7]  = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::busFold)), true);
    controls_[8]  = std::make_unique<Switch> (*dynamic_cast<juce::AudioParameterChoice*>  (apvts.getParameter (ParamIDs::busOsFactor)));
    controls_[9]  = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::outTrim)));
    controls_[10] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::outMix)));
    controls_[11] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxChorusRate)));
    controls_[12] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxChorusDepth)));
    controls_[13] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxChorusMix)));
    controls_[14] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxReverbRoom)));
    controls_[15] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxReverbDamp)));
    controls_[16] = std::make_unique<Knob>   (*dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::fxReverbMix)));
    for (auto& c : controls_)
    {
        if (c)
            addAndMakeVisible (*c);
    }

    // Dock: soundfont display + bank/program + load + peak meter.
    addAndMakeVisible (sfLabel_);
    sfLabel_.setText ("SOUNDFONT", juce::dontSendNotification);
    sfLabel_.setJustificationType (juce::Justification::centredLeft);
    sfLabel_.setColour (juce::Label::textColourId, juce::Colour (0xff9aa39b));
    sfLabel_.setFont (makeFont (9.5f, true));

    addAndMakeVisible (sfDisplay_);
    sfDisplay_.setJustificationType (juce::Justification::centredLeft);
    sfDisplay_.setColour (juce::Label::textColourId, theme::mint);
    sfDisplay_.setColour (juce::Label::backgroundColourId, theme::displayBg);
    sfDisplay_.setFont (makeFont (13.0f, true));
    refreshDisplay();

    addAndMakeVisible (bankDigits_);
    bankDigits_.setJustificationType (juce::Justification::centred);
    bankDigits_.setColour (juce::Label::textColourId, theme::mint);
    bankDigits_.setColour (juce::Label::backgroundColourId, theme::displayBg);
    bankDigits_.setFont (makeFont (14.0f, true));

    addAndMakeVisible (progDigits_);
    progDigits_.setJustificationType (juce::Justification::centred);
    progDigits_.setColour (juce::Label::textColourId, theme::mint);
    progDigits_.setColour (juce::Label::backgroundColourId, theme::displayBg);
    progDigits_.setFont (makeFont (14.0f, true));

    addAndMakeVisible (loadButton_);
    loadButton_.setColour (juce::TextButton::buttonColourId, theme::mintDeep);
    loadButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff06231b));
    loadButton_.onClick = [this] { onLoadButtonClicked(); };

    addAndMakeVisible (peakMeter_);

    addAndMakeVisible (keyboard_);
    keyboard_.setKeyRange (36, 36);   // C2..C5
    keyboard_.setNoteCallback ([this] (int note, bool on) {
        processor_.postNote (note, on, 100);
    });

    setSize (820, 600);

    startTimerHz (20);
}

RomplerEditor::~RomplerEditor() = default;

void RomplerEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::body2);
    auto face = getLocalBounds().reduced (8);
    g.setColour (theme::bodyEdge);
    g.drawRoundedRectangle (face.toFloat(), 14.0f, 2.0f);

    // Mint pinstripe under the top edge.
    g.setColour (theme::mint);
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) face.getX() + 14, (float) face.getY() + 10,
                                                    (float) face.getWidth() - 28, 2.0f), 1.0f);
}

void RomplerEditor::resized()
{
    auto b = getLocalBounds().reduced (14);

    auto top = b.removeFromTop (52);
    brandTitle_.setBounds (top.removeFromLeft (190).withY (4));
    brandSub_.setBounds (top.removeFromLeft (190).withY (30));
    badges_.setBounds (top.removeFromRight (180).reduced (4, 8));

    b.removeFromTop (8);

    // Two-column deck: VOICE and BUS boxes side by side.
    auto deck = b.removeFromTop (250);
    const int colW = (deck.getWidth() - 12) / 2;
    voiceBox_.setBounds (deck.withWidth (colW));
    busBox_.setBounds (deck.withX (colW + 12).withWidth (colW));

    layoutVoiceControls (voiceBox_.getLocalBounds().reduced (10, 12).withY (18));
    layoutBusControls (busBox_.getLocalBounds().reduced (10, 12).withY (18));

    b.removeFromTop (8);

    // FX box spans the full width.
    auto fx = b.removeFromTop (130);
    fxBox_.setBounds (fx);
    layoutFxControls (fxBox_.getLocalBounds().reduced (10, 12).withY (18));

    b.removeFromTop (8);

    // Dock.
    auto dock = b.removeFromTop (56);
    sfLabel_.setBounds (dock.withWidth (100).withY (2));
    sfDisplay_.setBounds (dock.withX (100).withY (16).withWidth (dock.getWidth() - 100 - 220));
    bankDigits_.setBounds (dock.withRightX (dock.getRight() - 200).withY (12).withWidth (80));
    progDigits_.setBounds (dock.withRightX (dock.getRight() - 110).withY (12).withWidth (80));
    loadButton_.setBounds (dock.withRightX (dock.getRight() - 8).withY (10).withWidth (96));
    peakMeter_.setBounds (dock.withX (dock.getWidth() - 340).withY (22).withWidth (90));

    b.removeFromTop (8);

    keyboard_.setBounds (b.reduced (0, 2));
}

void RomplerEditor::layoutVoiceControls (juce::Rectangle<int> area)
{
    // 3x2 grid: Drive | Vel->Drive | Filter Offset / Curve | Filter Route | Polyphony
    const auto rows = 2;
    const auto cols = 3;
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
    place (1, 1, 0);   // Curve
    place (3, 1, 1);   // Filter Route
    place (5, 1, 2);   // Polyphony
}

void RomplerEditor::layoutBusControls (juce::Rectangle<int> area)
{
    const auto rows = 2;
    const auto cols = 3;
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
    place (8, 0, 2);   // Oversample
    place (9, 1, 0);   // Out Trim
    place (10, 1, 1);  // Mix
    // Peak meter occupies the last cell.
    peakMeter_.setBounds (area.withX (area.getX() + 2 * cellW).withY (area.getY() + cellH)
                              .withSize (cellW, cellH).reduced (16, 20));
}

void RomplerEditor::layoutFxControls (juce::Rectangle<int> area)
{
    const auto count = 6;
    const auto cellW = area.getWidth() / count;
    for (int i = 0; i < count; ++i)
    {
        const int idx = 11 + i;
        if (controls_[static_cast<size_t> (idx)])
            controls_[static_cast<size_t> (idx)]->setBounds (
                area.withX (area.getX() + i * cellW).withY (0)
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
    const int count = processor_.getPresetCount();
    if (count > 0)
        sfDisplay_.setText (processor_.getPresetName (0), juce::dontSendNotification);
}

void RomplerEditor::onPresetChanged()
{
    // Placeholder: preset selection is driven by the dock readout in this
    // design; kept minimal.
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

    const auto [bank, program] = processor_.getCurrentBankProgram();
    bankDigits_.setText (juce::String (bank).paddedLeft ('0', 2), juce::dontSendNotification);
    progDigits_.setText (juce::String (program).paddedLeft ('0', 2), juce::dontSendNotification);
}

} // namespace aod
