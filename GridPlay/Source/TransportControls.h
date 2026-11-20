// Boutons de transport dessinés (lecture et stop exactement de la même taille)
// et réglage du tempo : glisser sur la valeur vers le haut ou le bas, molette,
// boutons - / + d'un BPM qui se répètent tant qu'on les tient, double-clic pour taper.
#pragma once

#include "Theme.h"

namespace gridplay
{
class TransportButton : public juce::Button
{
public:
    enum class Icon { play, stop };

    TransportButton (Icon i) : juce::Button (i == Icon::play ? "Lecture" : "Stop"), icon (i) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, findColour (juce::TextButton::buttonColourId), over, down);
        // Même boîte de 12 x 12 pour les deux icônes.
        const auto box = getLocalBounds().toFloat().withSizeKeepingCentre (12.0f, 12.0f);
        g.setColour (getToggleState() ? juce::Colours::black : theme::text);
        if (icon == Icon::play)
        {
            juce::Path p;
            p.addTriangle (box.getX() + 1.0f, box.getY(), box.getX() + 1.0f, box.getBottom(), box.getRight() + 1.0f, box.getCentreY());
            g.fillPath (p);
        }
        else
            g.fillRect (box.reduced (0.5f));
    }

private:
    Icon icon;
};

//==============================================================================
class TempoControl : public juce::Component
{
public:
    std::function<void (double)> onChange;
    static constexpr double minTempo = 20.0, maxTempo = 300.0;

    TempoControl()
    {
        for (auto* b : { &minus, &plus })
        {
            addAndMakeVisible (b);
            b->setRepeatSpeed (350, 60, 25);   // tenir le bouton : ça défile, de plus en plus vite
        }
        minus.onClick = [this] { nudge (-1.0); };
        plus.onClick = [this] { nudge (1.0); };
        minus.setTooltip ("-1 BPM (maintenir pour descendre ; Maj : -0,1)");
        plus.setTooltip ("+1 BPM (maintenir pour monter ; Maj : +0,1)");

        addAndMakeVisible (value);
        value.setJustificationType (juce::Justification::centred);
        value.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        value.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0d0d0d));
        value.setColour (juce::Label::outlineColourId, theme::rule);
        value.setColour (juce::Label::textColourId, theme::text);
        value.setEditable (false, true, false);
        value.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        value.setTooltip (juce::String::fromUTF8 ("Tempo : cliquer et glisser vers le haut ou le bas (Maj : au centième), "
                                                  "molette, ou double-clic pour taper la valeur"));
        value.addMouseListener (this, false);
        value.onEditorShow = [this]
        {
            if (auto* editor = value.getCurrentTextEditor())
            {
                editor->setText (juce::String (tempo, 2), false);
                editor->selectAll();
                editor->setInputRestrictions (6, "0123456789.,");
            }
        };
        value.onTextChange = [this]
        {
            const auto typed = value.getText().replaceCharacter (',', '.').getDoubleValue();
            if (typed > 0.0)
                set (typed);
            else
                refresh();
        };
        refresh();
    }

    void setTempo (double t)
    {
        if (dragging || value.isBeingEdited() || std::abs (t - tempo) < 1.0e-9)
            return;
        tempo = t;
        refresh();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        plus.setBounds (r.removeFromRight (24));
        r.removeFromRight (2);
        minus.setBounds (r.removeFromRight (24));
        r.removeFromRight (4);
        value.setBounds (r);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.eventComponent != &value)
            return;
        dragStartTempo = tempo;
        dragging = true;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.eventComponent != &value)
            return;
        // Vers le haut : plus rapide. 1 BPM tous les 4 pixels (0,01 avec Maj).
        const double perPixel = e.mods.isShiftDown() ? 0.01 : 0.25;
        set (dragStartTempo - e.getDistanceFromDragStartY() * perPixel, ! e.mods.isShiftDown());
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &value)
            dragging = false;
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.eventComponent != &value || std::abs (w.deltaY) < 1.0e-4f)
            return;
        nudge (w.deltaY > 0 ? 1.0 : -1.0);
    }

private:
    void nudge (double steps)
    {
        const bool fine = juce::ModifierKeys::currentModifiers.isShiftDown();
        set (tempo + steps * (fine ? 0.1 : 1.0), false);
    }

    void set (double t, bool roundToWhole = false)
    {
        if (roundToWhole)
            t = std::round (t);
        t = juce::jlimit (minTempo, maxTempo, std::round (t * 100.0) / 100.0);
        if (std::abs (t - tempo) < 1.0e-9)
            return;
        tempo = t;
        refresh();
        if (onChange)
            onChange (tempo);
    }

    void refresh() { value.setText (juce::String (tempo, 2) + " BPM", juce::dontSendNotification); }

    juce::TextButton minus { "-" }, plus { "+" };
    juce::Label value;
    double tempo = 120.0, dragStartTempo = 120.0;
    bool dragging = false;
};
} // namespace gridplay
