// Couleurs et style de Grid Play : gris sombre façon Ableton, orange Load & Play.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace gridplay::theme
{
const juce::Colour background   { 0xff1c1c1c };
const juce::Colour panel        { 0xff262626 };
const juce::Colour panelLight   { 0xff333333 };
const juce::Colour slot         { 0xff2e2e2e };
const juce::Colour line         { 0xff151515 };
const juce::Colour text         { 0xffdedede };
const juce::Colour textDim      { 0xff8c8c8c };
const juce::Colour accent       { 0xffe8862c };
const juce::Colour play         { 0xff7fdc5a };
const juce::Colour record       { 0xffff4b3e };
const juce::Colour selection    { 0xffffffff };

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColourScheme ({ panel, background, panelLight, textDim, text, accent, juce::Colours::black, panelLight, text });
        setColour (juce::ComboBox::backgroundColourId, panelLight);
        setColour (juce::ComboBox::outlineColourId, line);
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
        return juce::Font (juce::FontOptions (juce::jmin (13.0f, buttonHeight * 0.6f), juce::Font::bold));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override        { return juce::Font (juce::FontOptions (12.5f)); }
    juce::Font getPopupMenuFont() override                        { return juce::Font (juce::FontOptions (14.0f)); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& colour, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : colour;
        if (down) c = c.darker (0.2f);
        else if (over) c = c.brighter (0.12f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.5f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        juce::Path arrow;
        const float cx = (float) width - 9.0f, cy = (float) height * 0.5f;
        arrow.addTriangle (cx - 3.5f, cy - 2.0f, cx + 3.5f, cy - 2.0f, cx, cy + 2.5f);
        g.setColour (textDim);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (1, 1, box.getWidth() - 16, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};
} // namespace gridplay::theme
