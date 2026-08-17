#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace eon::theme
{

/**
    The EON-50 panel palette.

    A dark chassis carrying a cream control surface, grouped into mint-outlined
    boxes — the microKORG's control-panel arrangement. Mint is the interface
    accent and does all the structural work (outlines, pinstripe, LCD, active
    states), so the four controls that shape distortion are marked in orange
    instead: if those knob caps were mint too they would disappear into the
    chrome around them.
*/

inline const juce::Colour body           { 0xff232622 };
inline const juce::Colour bodyDark       { 0xff171916 };
inline const juce::Colour bodyEdge       { 0xff34382f };

inline const juce::Colour panel          { 0xffded7bd };
inline const juce::Colour panelEdge      { 0xffb6ae8f };

inline const juce::Colour ink            { 0xff221f1a };
inline const juce::Colour inkSoft        { 0xff56503f };

inline const juce::Colour mint           { 0xff7fe0c4 };
inline const juce::Colour mintDeep       { 0xff34a984 };
inline const juce::Colour mintShadow     { 0xff1f7a5f };
inline const juce::Colour onMint         { 0xff06231b };

inline const juce::Colour hot            { 0xffff9452 };
inline const juce::Colour hotDeep        { 0xffc96324 };
inline const juce::Colour onHot          { 0xff4a2408 };

inline const juce::Colour capCream       { 0xfff2ecd8 };
inline const juce::Colour capCreamShadow { 0xffb7ae8e };

inline const juce::Colour lcdBackground  { 0xff0e1613 };
inline const juce::Colour chassisText    { 0xff9aa39b };

inline const juce::Colour ledRed         { 0xffff4d3d };
inline const juce::Colour ledHot         { 0xffffb454 };
inline const juce::Colour ledMint        { 0xff7fe0c4 };
inline const juce::Colour ledOff         { 0xff2a2e28 };

/** Condensed sans for silkscreened panel labels. */
[[nodiscard]] inline juce::Font labelFont (float height, bool boldFace = true)
{
    return juce::Font (juce::FontOptions ("Arial Narrow", height,
                                          boldFace ? juce::Font::bold : juce::Font::plain));
}

/** Monospace, for anything drawn as a lit readout. */
[[nodiscard]] inline juce::Font lcdFont (float height, bool boldFace = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height,
                                          boldFace ? juce::Font::bold : juce::Font::plain));
}

/**
    Draws text with letter spacing, centred in `area`.

    JUCE has no tracking on Font, and the wide-spaced uppercase label is most of
    what makes a panel read as silkscreened rather than as a UI caption — so the
    glyphs are placed one at a time.
*/
inline void drawTrackedText (juce::Graphics& g,
                             const juce::String& text,
                             juce::Rectangle<int> area,
                             const juce::Font& font,
                             float tracking,
                             juce::Justification justification = juce::Justification::centred)
{
    if (text.isEmpty())
        return;

    g.setFont (font);

    float total = 0.0f;
    for (auto character : text)
        total += juce::GlyphArrangement::getStringWidth (font, juce::String::charToString (character)) + tracking;
    total -= tracking;

    const auto bounds = area.toFloat();
    float x = bounds.getX();

    if (justification.testFlags (juce::Justification::horizontallyCentred))
        x = bounds.getCentreX() - total * 0.5f;
    else if (justification.testFlags (juce::Justification::right))
        x = bounds.getRight() - total;

    const float baseline = bounds.getCentreY() + font.getAscent() * 0.5f - font.getDescent() * 0.25f;

    for (auto character : text)
    {
        const auto glyph = juce::String::charToString (character);
        g.drawSingleLineText (glyph, juce::roundToInt (x), juce::roundToInt (baseline));
        x += juce::GlyphArrangement::getStringWidth (font, glyph) + tracking;
    }
}

/** The recessed dark window every lit readout sits in. */
inline void drawLcdWell (juce::Graphics& g, juce::Rectangle<float> area, float corner = 3.0f)
{
    g.setColour (lcdBackground);
    g.fillRoundedRectangle (area, corner);

    g.setColour (mint.withAlpha (0.14f));
    g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
}

} // namespace eon::theme
