// Couleurs et style de Grid Play : gris sombre façon Ableton, orange Load & Play.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#if __has_include (<BinaryData.h>)
 #include <BinaryData.h>
#endif

namespace gridplay::theme
{
// Mêmes teintes que les plugins Load & Play : noir chaud, blanc cassé, bois, orange.
const juce::Colour background   { 0xff121212 };
const juce::Colour panel        { 0xff1b1b1b };
const juce::Colour panelLight   { 0xff2b2a28 };
const juce::Colour slot         { 0xff232220 };
const juce::Colour line         { 0xff0b0b0b };
const juce::Colour rule         { 0xff343434 };   // filets, comme sous l'en-tête des plugins
const juce::Colour text         { 0xfff0ede4 };
const juce::Colour textDim      { 0xff8c877d };
const juce::Colour wood         { 0xff5a3418 };
const juce::Colour accent       { 0xffe8862c };
const juce::Colour play         { 0xff8fd35a };
const juce::Colour record       { 0xffff5a4a };
const juce::Colour selection    { 0xfff0ede4 };

inline juce::Font font (float size, int style = juce::Font::plain)
{
    return juce::Font (juce::FontOptions (size, style));
}

// Titre de section façon plugin : capitales grises, lettres espacées (passer le texte en capitales).
inline void sectionTitle (juce::Graphics& g, const juce::String& label, juce::Rectangle<int> area,
                          juce::Justification j = juce::Justification::centredLeft)
{
    g.setColour (textDim);
    g.setFont (font (10.5f, juce::Font::bold).withExtraKerningFactor (0.08f));
    g.drawText (label, area, j, true);   // le texte est déjà en capitales (accents compris)
}

// Bandes de bois, comme les joues des plugins.
inline void woodCheek (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (wood.brighter (0.12f), r.getX(), 0.0f, wood.darker (0.35f), r.getRight(), 0.0f, false));
    g.fillRect (r);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (float y = r.getY() + 7.0f; y < r.getBottom(); y += 23.0f)
        g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Source Sans 3 partout (sauf les textes en police à chasse fixe).
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    {
        if (f.getTypefaceName() != juce::Font::getDefaultSansSerifFontName() || regular == nullptr)
            return juce::LookAndFeel_V4::getTypefaceForFont (f);
        if (f.isItalic()) return italic;
        if (f.isBold()) return bold;
        return regular;
    }

    LookAndFeel()
    {
        regular = juce::Typeface::createSystemTypefaceFor (BinaryData::SourceSans3Regular_ttf, BinaryData::SourceSans3Regular_ttfSize);
        bold = juce::Typeface::createSystemTypefaceFor (BinaryData::SourceSans3Bold_ttf, BinaryData::SourceSans3Bold_ttfSize);
        italic = juce::Typeface::createSystemTypefaceFor (BinaryData::SourceSans3It_ttf, BinaryData::SourceSans3It_ttfSize);
        if (bold == nullptr) bold = regular;
        if (italic == nullptr) italic = regular;
        setColourScheme ({ panel, background, panelLight, textDim, text, accent, juce::Colours::black, panelLight, text });
        setColour (juce::ComboBox::backgroundColourId, panelLight);
        setColour (juce::ComboBox::outlineColourId, rule);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, textDim);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff2b2b2b));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::TextButton::buttonColourId, panelLight);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff3a3a3a));
        setColour (juce::Slider::trackColourId, accent);
        setColour (juce::Slider::thumbColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, panelLight);
        setColour (juce::Label::textColourId, text);
        setColour (juce::TextEditor::backgroundColourId, panelLight);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.5f));
        setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff4a4a4a));
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, text);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff3a3a3a));
        setColour (juce::TooltipWindow::textColourId, text);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (12.5f, buttonHeight * 0.58f), juce::Font::bold)).withExtraKerningFactor (0.04f);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override        { return juce::Font (juce::FontOptions (12.5f, juce::Font::bold)); }
    juce::Font getPopupMenuFont() override                        { return juce::Font (juce::FontOptions (14.0f)); }

    // Boutons des plugins : léger relief (haut plus clair), coins arrondis, filet sombre.
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& colour, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : colour;
        if (down) c = c.darker (0.25f);
        else if (over) c = c.brighter (0.1f);
        g.setGradientFill (juce::ColourGradient (c.brighter (0.08f), 0.0f, r.getY(), c.darker (0.12f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (b.getToggleState() ? 0.25f : 0.06f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.5f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path arrow;
        const float cx = (float) width - 9.0f, cy = (float) height * 0.5f;
        arrow.addTriangle (cx - 3.5f, cy - 2.0f, cx + 3.5f, cy - 2.0f, cx, cy + 2.5f);
        g.setColour (textDim);
        g.fillPath (arrow);
    }

    // Potards des plugins Load & Play : jupe noire striée, chapeau alu, index blanc.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
        const auto centre = bounds.getCentre();
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 5.0f;

        g.setColour (textDim);
        for (int i = 0; i < 11; ++i)
        {
            const float angle = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
            g.drawLine ({ centre.getPointOnCircumference (radius + 1.5f, angle), centre.getPointOnCircumference (radius + 4.0f, angle) }, 1.0f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), centre.translated (-radius * 0.5f, -radius * 0.5f),
                                                 juce::Colour (0xff050505), centre.translated (radius, radius), true));
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
        g.setColour (juce::Colours::black);
        for (int i = 0; i < 28; ++i)
        {
            const float angle = juce::MathConstants<float>::twoPi * (float) i / 28.0f;
            g.drawLine ({ centre.getPointOnCircumference (radius * 0.8f, angle), centre.getPointOnCircumference (radius, angle) }, 1.0f);
        }
        const float cap = radius * 0.52f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f0f0), centre.translated (-cap, -cap),
                                                 juce::Colour (0xff767676), centre.translated (cap, cap), false));
        g.fillEllipse (juce::Rectangle<float> (cap * 2.0f, cap * 2.0f).withCentre (centre));
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        g.setColour (slider.isEnabled() ? text : textDim);
        g.drawLine ({ centre.getPointOnCircumference (cap * 1.05f, angle), centre.getPointOnCircumference (radius * 0.95f, angle) }, 2.0f);
    }

    // Fader vertical : rainure sombre et chapeau gris, comme une table de mixage.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, slider);
            return;
        }
        const auto r = juce::Rectangle<int> (x, y, width, height).toFloat();
        g.setColour (juce::Colour (0xff0b0b0b));
        g.fillRoundedRectangle (r.withSizeKeepingCentre (5.0f, r.getHeight()), 2.5f);
        g.setColour (textDim.withAlpha (0.6f));
        for (int i = 0; i <= 10; ++i)
        {
            const float ty = r.getY() + r.getHeight() * (float) i / 10.0f;
            g.fillRect (r.getCentreX() - 14.0f, ty, 6.0f, 1.0f);
            g.fillRect (r.getCentreX() + 8.0f, ty, 6.0f, 1.0f);
        }
        const auto capArea = juce::Rectangle<float> (r.getCentreX() - 13.0f, sliderPos - 9.0f, 26.0f, 18.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffdcdad2), 0.0f, capArea.getY(), juce::Colour (0xff6c6a64), 0.0f, capArea.getBottom(), false));
        g.fillRoundedRectangle (capArea, 2.5f);
        g.setColour (juce::Colours::black);
        g.fillRect (capArea.getX() + 2.0f, capArea.getCentreY() - 0.75f, capArea.getWidth() - 4.0f, 1.5f);
    }

    juce::Typeface::Ptr regular, bold, italic;

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (1, 1, box.getWidth() - 16, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};
} // namespace gridplay::theme
