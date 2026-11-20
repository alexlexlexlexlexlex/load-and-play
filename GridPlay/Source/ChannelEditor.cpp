#include "ChannelEditor.h"

namespace gridplay
{
namespace
{
    const juce::Colour bandColours[4] = { juce::Colour (0xffe8862c), juce::Colour (0xfff2d34f), juce::Colour (0xff8fd35a), juce::Colour (0xff6fb7ff) };

    // Types possibles pour chaque bande, comme dans Cubase (les bandes 1 et 4 peuvent couper).
    std::vector<EqType> typesForBand (int band)
    {
        if (band == 0) return { EqType::lowShelf, EqType::peak, EqType::lowCut };
        if (band == 3) return { EqType::highShelf, EqType::peak, EqType::highCut };
        return { EqType::peak, EqType::lowShelf, EqType::highShelf };
    }

    juce::String formatFrequency (double f)
    {
        return f < 1000.0 ? juce::String (juce::roundToInt (f)) : juce::String (f / 1000.0, f < 10000.0 ? 2 : 1) + "k";
    }

    bool isCut (EqType t) { return t == EqType::lowCut || t == EqType::highCut; }

    void styleKnob (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 16);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxTextColourId, theme::text);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    }
}

//==============================================================================
// La courbe de l'égaliseur : 20 Hz à 20 kHz, ±24 dB ; un point par bande à tirer.
class ChannelEditor::EqGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit EqGraph (ChannelEditor& o) : owner (o)
    {
        setTooltip (juce::String::fromUTF8 ("Tirer un point : fréquence et gain. Molette sur un point : largeur (Q). "
                                            "Double-clic sur un point : remettre à 0 dB."));
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0b0b0b));
        g.fillRoundedRectangle (r, 5.0f);

        // Grille : fréquences et gains.
        g.setFont (theme::font (9.5f));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            const float x = xFor (f);
            g.setColour (juce::Colour (0xff222120));
            g.fillRect (x, r.getY(), 1.0f, r.getHeight());
            g.setColour (theme::textDim);
            g.drawText (f < 1000.0 ? juce::String ((int) f) : juce::String ((int) (f / 1000.0)) + "k", (int) x + 3, (int) r.getBottom() - 14, 30, 12,
                        juce::Justification::centredLeft);
        }
        for (float db : { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f })
        {
            const float y = yFor (db);
            g.setColour (db == 0.0f ? juce::Colour (0xff3a3733) : juce::Colour (0xff1c1b1a));
            g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
            g.setColour (theme::textDim);
            g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), (int) r.getX() + 4, (int) y - 12, 30, 11, juce::Justification::centredLeft);
        }

        // La courbe totale.
        const auto& shown = owner.currentEq;
        juce::Path curve;
        for (int i = 0; i <= 240; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / 240.0f;
            const float y = juce::jlimit (r.getY(), r.getBottom(), yFor (eqResponseDb (shown, freqFor (x), 48000.0)));
            if (i == 0) curve.startNewSubPath (x, y);
            else curve.lineTo (x, y);
        }
        juce::Path fill (curve);
        fill.lineTo (r.getRight(), yFor (0.0f));
        fill.lineTo (r.getX(), yFor (0.0f));
        fill.closeSubPath();
        g.setColour (theme::accent.withAlpha (shown.bypass ? 0.04f : 0.12f));
        g.fillPath (fill);
        g.setColour (shown.bypass ? theme::textDim : theme::accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        // Les points.
        for (int b = 0; b < 4; ++b)
        {
            const auto p = pointFor (b);
            const bool on = shown.bands[(size_t) b].on;
            const float radius = b == hover || b == dragging ? 9.0f : 7.5f;
            g.setColour (on ? bandColours[b] : juce::Colour (0xff4a4640));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (p));
            g.setColour (juce::Colours::black);
            g.setFont (theme::font (10.5f, juce::Font::bold));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (p), juce::Justification::centred);
        }

        if (shown.bypass)
        {
            g.setColour (theme::textDim);
            g.setFont (theme::font (12.0f, juce::Font::bold));
            g.drawText (juce::String::fromUTF8 ("EQ COUPÉ"), getLocalBounds().reduced (10), juce::Justification::topRight);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int b = pointAt (e.position);
        if (b != hover)
        {
            hover = b;
            repaint();
        }
        setMouseCursor (b >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = pointAt (e.position);
        if (dragging >= 0 && ! owner.currentEq.bands[(size_t) dragging].on)
            owner.editEq ([b = dragging] (ChannelEq& target) { target.bands[(size_t) b].on = true; });
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging < 0)
            return;
        const double f = juce::jlimit (20.0, 20000.0, freqFor (e.position.x));
        const float gain = juce::jlimit (-24.0f, 24.0f, gainFor (e.position.y));
        owner.editEq ([b = dragging, f, gain] (ChannelEq& target)
        {
            auto& band = target.bands[(size_t) b];
            band.frequency = (float) f;
            if (! isCut (band.type))
                band.gain = std::round (gain * 10.0f) / 10.0f;
        });
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = -1; repaint(); }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int b = pointAt (e.position);
        if (b >= 0)
            owner.editEq ([b] (ChannelEq& target) { target.bands[(size_t) b].gain = 0.0f; });
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int b = pointAt (e.position);
        if (b < 0 || std::abs (w.deltaY) < 1.0e-4f)
            return;
        owner.editEq ([b, up = w.deltaY > 0] (ChannelEq& target)
        {
            auto& q = target.bands[(size_t) b].q;
            q = juce::jlimit (0.1f, 18.0f, q * (up ? 1.12f : 1.0f / 1.12f));
        });
    }

private:
    float xFor (double f) const      { return (float) (std::log (f / 20.0) / std::log (1000.0) * getWidth()); }
    double freqFor (float x) const   { return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) x / getWidth())); }
    float yFor (float db) const      { return getHeight() * 0.5f - db / 24.0f * (getHeight() * 0.5f - 10.0f); }
    float gainFor (float y) const    { return (getHeight() * 0.5f - y) / (getHeight() * 0.5f - 10.0f) * 24.0f; }

    juce::Point<float> pointFor (int b) const
    {
        const auto& band = owner.currentEq.bands[(size_t) b];
        return { xFor (band.frequency), yFor (isCut (band.type) ? 0.0f : band.gain) };
    }

    int pointAt (juce::Point<float> p) const
    {
        int best = -1;
        float bestDistance = 14.0f;
        for (int b = 0; b < 4; ++b)
            if (const float d = pointFor (b).getDistanceFrom (p); d < bestDistance)
            {
                bestDistance = d;
                best = b;
            }
        return best;
    }

    ChannelEditor& owner;
    int hover = -1, dragging = -1;
};

//==============================================================================
// Une case d'insert (1 à 8) : marche / arrêt, nom (clic : ouvrir l'effet ; case vide :
// choisir un effet), menu. On glisse le nom sur une autre case pour changer l'ordre.
class ChannelEditor::InsertRow : public juce::Component, public juce::DragAndDropTarget
{
public:
    InsertRow (ChannelEditor& o, int index) : owner (o), slot (index)
    {
        power.setClickingTogglesState (false);
        power.setColour (juce::TextButton::buttonOnColourId, theme::accent);
        power.setTooltip ("Activer / contourner l'effet (bypass)");
        power.onClick = [this]
        {
            owner.remember();
            const int t = owner.target();
            owner.engine.edit ([&] (Session& session)
            {
                auto& slots = t == AudioEngine::masterTarget ? session.song.masterInserts : session.song.tracks[(size_t) t].inserts;
                if (slot < (int) slots.size())
                    slots[(size_t) slot].bypass = ! slots[(size_t) slot].bypass;
                return 0;
            });
        };
        nameButton.onClick = [this]
        {
            if (dragged) { dragged = false; return; }
            if (filled) { if (owner.onOpenInsert) owner.onOpenInsert (owner.target(), slot); }
            else owner.showInsertMenu (slot, false);
        };
        nameButton.addMouseListener (this, false);
        menu.setButtonText (juce::String::fromUTF8 ("▾"));
        menu.setTooltip (juce::String::fromUTF8 ("Remplacer, déplacer ou retirer l'effet"));
        menu.onClick = [this] { owner.showInsertMenu (slot, filled); };
        for (auto* b : { &power, &nameButton, &menu })
            addAndMakeVisible (b);
    }

    void show (bool isFilled, const juce::String& label, bool bypassed)
    {
        filled = isFilled;
        power.setVisible (filled);
        menu.setVisible (filled);
        power.setButtonText (juce::String::fromUTF8 ("⏻"));
        power.setToggleState (filled && ! bypassed, juce::dontSendNotification);
        nameButton.setButtonText (filled ? label : juce::String ("+"));
        nameButton.setTooltip (filled ? juce::String::fromUTF8 ("Clic : ouvrir l'effet. Glisser sur une autre case : changer sa place dans la chaîne")
                                      : juce::String::fromUTF8 ("Choisir un effet pour cette case"));
        nameButton.setColour (juce::TextButton::buttonColourId, filled ? theme::panelLight : theme::slot);
        nameButton.setColour (juce::TextButton::textColourOffId, filled ? (bypassed ? theme::textDim : theme::text) : theme::textDim);
        resized();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (12.0f, juce::Font::bold));
        g.drawText (juce::String (slot + 1), 0, 0, 16, getHeight(), juce::Justification::centred);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (dropHover)
        {
            g.setColour (theme::accent);
            g.drawRoundedRectangle (getLocalBounds().toFloat().withTrimmedLeft (18).reduced (1.0f), 4.0f, 2.0f);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().withTrimmedLeft (20);
        if (filled)
        {
            power.setBounds (r.removeFromLeft (28));
            r.removeFromLeft (3);
            menu.setBounds (r.removeFromRight (24));
            r.removeFromRight (3);
        }
        nameButton.setBounds (r);
    }

    // Glisser le nom d'un effet vers une autre case.
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! filled || e.eventComponent != &nameButton || e.getDistanceFromDragStart() < 6 || owner.isDragAndDropActive())
            return;
        dragged = true;
        owner.startDragging (slot, this, juce::ScaledImage (createComponentSnapshot (getLocalBounds())));
    }

    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return dynamic_cast<InsertRow*> (d.sourceComponent.get()) != nullptr && d.description.isInt();
    }
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        dropHover = false;
        repaint();
        const int from = (int) d.description;
        if (from != slot)
        {
            owner.rememberMove (from, slot);
            owner.engine.moveInsert (owner.target(), from, slot);
        }
        owner.refresh();
    }

private:
    ChannelEditor& owner;
    int slot;
    bool filled = false, dropHover = false, dragged = false;
    juce::TextButton power, nameButton, menu;
};

//==============================================================================
// Pictogramme d'un type de bande (cloche, plateau, coupe), à la place du texte.
class ChannelEditor::TypeButton : public juce::Button
{
public:
    TypeButton (EqType t, juce::Colour c) : juce::Button (juce::String::fromUTF8 (eqTypeName (t))), type (t), colour (c)
    {
        setTooltip (juce::String::fromUTF8 (eqTypeName (t)));
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (getToggleState() ? colour : (over ? theme::panelLight.brighter (0.1f) : theme::panelLight));
        if (down) g.setColour (colour.darker (0.2f));
        g.fillRoundedRectangle (r, 4.0f);
        const auto ink = getToggleState() ? juce::Colours::black : theme::text;
        const auto a = r.reduced (5.0f, 6.0f);
        const float x0 = a.getX(), x1 = a.getRight(), top = a.getY(), mid = a.getCentreY(), bot = a.getBottom();
        juce::Path p;
        switch (type)
        {
            case EqType::peak:
                p.startNewSubPath (x0, mid + 3.0f);
                p.cubicTo (a.getCentreX() - 4.0f, mid + 3.0f, a.getCentreX() - 3.0f, top, a.getCentreX(), top);
                p.cubicTo (a.getCentreX() + 3.0f, top, a.getCentreX() + 4.0f, mid + 3.0f, x1, mid + 3.0f);
                break;
            case EqType::lowShelf:
                p.startNewSubPath (x0, top + 1.0f);
                p.lineTo (a.getCentreX() - 3.0f, top + 1.0f);
                p.cubicTo (a.getCentreX(), top + 1.0f, a.getCentreX(), mid + 3.0f, a.getCentreX() + 4.0f, mid + 3.0f);
                p.lineTo (x1, mid + 3.0f);
                break;
            case EqType::highShelf:
                p.startNewSubPath (x0, mid + 3.0f);
                p.lineTo (a.getCentreX() - 4.0f, mid + 3.0f);
                p.cubicTo (a.getCentreX(), mid + 3.0f, a.getCentreX(), top + 1.0f, a.getCentreX() + 3.0f, top + 1.0f);
                p.lineTo (x1, top + 1.0f);
                break;
            case EqType::lowCut:
                p.startNewSubPath (x0 + 2.0f, bot);
                p.cubicTo (a.getCentreX() - 2.0f, top + 1.0f, a.getCentreX(), top + 1.0f, a.getCentreX() + 2.0f, top + 1.0f);
                p.lineTo (x1, top + 1.0f);
                break;
            case EqType::highCut:
                p.startNewSubPath (x0, top + 1.0f);
                p.lineTo (a.getCentreX() - 2.0f, top + 1.0f);
                p.cubicTo (a.getCentreX(), top + 1.0f, a.getCentreX() + 2.0f, top + 1.0f, x1 - 2.0f, bot);
                break;
        }
        g.setColour (ink);
        g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    EqType type;

private:
    juce::Colour colour;
};

//==============================================================================
// Les réglages d'une bande : marche, type, fréquence, gain, Q.
class ChannelEditor::BandControls : public juce::Component
{
public:
    BandControls (ChannelEditor& o, int b) : owner (o), band (b)
    {
        onButton.setButtonText (juce::String (b + 1));
        onButton.setColour (juce::TextButton::buttonOnColourId, bandColours[b]);
        onButton.setTooltip ("Bande " + juce::String (b + 1) + juce::String::fromUTF8 (" : marche / arrêt"));
        onButton.onClick = [this] { owner.editEq ([this] (ChannelEq& target) { target.bands[(size_t) band].on = ! target.bands[(size_t) band].on; }); };
        addAndMakeVisible (onButton);

        for (auto t : typesForBand (b))
        {
            auto* button = types.add (new TypeButton (t, bandColours[b]));
            button->onClick = [this, t] { owner.editEq ([this, t] (ChannelEq& target) { target.bands[(size_t) band].type = t; }); };
            addAndMakeVisible (button);
        }

        for (auto* s : { &frequency, &gain, &q })
        {
            styleKnob (*s);
            addAndMakeVisible (s);
        }
        frequency.setRange (20.0, 20000.0);
        frequency.setSkewFactorFromMidPoint (1000.0);
        frequency.textFromValueFunction = [] (double v) { return formatFrequency (v); };
        frequency.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue() * (t.containsIgnoreCase ("k") ? 1000.0 : 1.0); };
        gain.setRange (-24.0, 24.0, 0.1);
        gain.setDoubleClickReturnValue (true, 0.0);
        gain.textFromValueFunction = [] (double v) { return (v > 0.0 ? "+" : "") + juce::String (v, 1); };
        q.setRange (0.1, 18.0);
        q.setSkewFactorFromMidPoint (1.0);
        q.textFromValueFunction = [] (double v) { return juce::String (v, v < 10.0 ? 2 : 1); };
        frequency.setTooltip (juce::String::fromUTF8 ("Fréquence"));
        gain.setTooltip ("Gain (double-clic : 0 dB)");
        q.setTooltip ("Largeur de la bande (Q)");
        frequency.onValueChange = [this] { const auto v = (float) frequency.getValue(); owner.editEq ([this, v] (ChannelEq& target) { target.bands[(size_t) band].frequency = v; }); };
        gain.onValueChange = [this] { const auto v = (float) gain.getValue(); owner.editEq ([this, v] (ChannelEq& target) { target.bands[(size_t) band].gain = v; }); };
        q.onValueChange = [this] { const auto v = (float) q.getValue(); owner.editEq ([this, v] (ChannelEq& target) { target.bands[(size_t) band].q = v; }); };
    }

    void show (const EqBand& b)
    {
        onButton.setToggleState (b.on, juce::dontSendNotification);
        for (auto* button : types)
            button->setToggleState (button->type == b.type, juce::dontSendNotification);
        auto set = [] (juce::Slider& slider, double v) { if (! slider.isMouseButtonDown() && std::abs (slider.getValue() - v) > 1.0e-4) slider.setValue (v, juce::dontSendNotification); };
        set (frequency, b.frequency);
        set (gain, b.gain);
        set (q, b.q);
        gain.setEnabled (! isCut (b.type));
        q.setEnabled (! isCut (b.type));
    }

    void paint (juce::Graphics& g) override
    {
        // Légendes des potards, en capitales comme sur les plugins.
        const int w = getWidth() / 3;
        const char* captions[] = { "FRÉQ", "GAIN", "Q" };
        for (int i = 0; i < 3; ++i)
            theme::sectionTitle (g, juce::String::fromUTF8 (captions[i]), { i * w, 28, w, 12 }, juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (22);
        onButton.setBounds (top.removeFromLeft (26));
        top.removeFromLeft (4);
        const int bw = (top.getWidth() - 4) / juce::jmax (1, types.size());
        for (auto* button : types)
        {
            button->setBounds (top.removeFromLeft (bw));
            top.removeFromLeft (2);
        }
        r.removeFromTop (20);
        const int w = r.getWidth() / 3;
        frequency.setBounds (r.removeFromLeft (w));
        gain.setBounds (r.removeFromLeft (w));
        q.setBounds (r);
    }

private:
    ChannelEditor& owner;
    int band;
    juce::TextButton onButton;
    juce::OwnedArray<TypeButton> types;
    juce::Slider frequency, gain, q;
};

//==============================================================================
ChannelEditor::ChannelEditor (AudioEngine& e, int id) : engine (e), trackId (id)
{
    graph = std::make_unique<EqGraph> (*this);
    addAndMakeVisible (*graph);
    for (int i = 0; i < maxInserts; ++i)
        addAndMakeVisible (rows.add (new InsertRow (*this, i)));
    for (int b = 0; b < 4; ++b)
        addAndMakeVisible (bands.add (new BandControls (*this, b)));

    eqBypass.setClickingTogglesState (false);
    eqBypass.setColour (juce::TextButton::buttonOnColourId, theme::accent);
    eqBypass.setTooltip (juce::String::fromUTF8 ("Égaliseur en marche / coupé"));
    eqBypass.onClick = [this] { editEq ([] (ChannelEq& target) { target.bypass = ! target.bypass; }); };
    eqReset.setButtonText (juce::String::fromUTF8 ("Réinitialiser"));
    eqReset.setTooltip (juce::String::fromUTF8 ("Remettre l'égaliseur à plat"));
    eqReset.onClick = [this] { editEq ([] (ChannelEq& target) { target = ChannelEq(); }); };
    addAndMakeVisible (eqBypass);
    addAndMakeVisible (eqReset);

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange (0.0, 1.0);
    fader.setDoubleClickReturnValue (true, 0.85);
    fader.setTooltip ("Volume (double-clic : 0 dB)");
    fader.onDragStart = [this] { remember(); };
    fader.onValueChange = [this]
    {
        const float v = (float) fader.getValue();
        const int t = target();
        engine.edit ([&] (Session& s)
        {
            if (t == AudioEngine::masterTarget) s.song.masterVolume = v;
            else if (juce::isPositiveAndBelow (t, s.song.numTracks())) s.song.tracks[(size_t) t].volume = v;
            return 0;
        });
    };
    addAndMakeVisible (fader);

    // Panoramique en barre, comme sur une voie de Cubase (« C » au centre).
    pan.setSliderStyle (juce::Slider::LinearBar);
    pan.setColour (juce::Slider::trackColourId, theme::accent.withAlpha (0.45f));
    pan.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff0d0d0d));
    pan.onDragStart = [this] { remember(); };
    pan.setRange (-1.0, 1.0, 0.01);
    pan.setDoubleClickReturnValue (true, 0.0);
    pan.textFromValueFunction = [] (double v)
    {
        if (std::abs (v) < 0.005) return juce::String ("C");
        return (v < 0 ? "G " : "D ") + juce::String (juce::roundToInt (std::abs (v) * 100.0));
    };
    pan.setTooltip ("Panoramique (double-clic : au centre)");
    pan.updateText();
    pan.onValueChange = [this]
    {
        const float v = (float) pan.getValue();
        const int t = target();
        engine.edit ([&] (Session& s) { if (juce::isPositiveAndBelow (t, s.song.numTracks())) s.song.tracks[(size_t) t].pan = v; return 0; });
    };
    pan.setVisible (trackId != masterId);
    addAndMakeVisible (pan);

    for (auto* l : { &faderValue, &peakValue })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (theme::font (12.0f, juce::Font::bold));
        l->setColour (juce::Label::backgroundColourId, juce::Colour (0xff0d0d0d));
        l->setColour (juce::Label::outlineColourId, theme::rule);
        addAndMakeVisible (l);
    }
    faderValue.setEditable (false, true, false);
    faderValue.setTooltip (juce::String::fromUTF8 ("Volume en dB (double-clic pour taper une valeur)"));
    faderValue.onTextChange = [this]
    {
        remember();
        const float db = faderValue.getText().upToFirstOccurrenceOf (" ", false, false).replaceCharacter (',', '.').getFloatValue();
        fader.setValue (juce::jlimit (0.0, 1.0, (double) (0.85f * std::sqrt (juce::Decibels::decibelsToGain (db)))), juce::sendNotificationSync);
    };
    peakValue.setTooltip (juce::String::fromUTF8 ("Crête maximale (clic : remettre à zéro)"));
    peakValue.addMouseListener (this, false);

    // En-tête : annuler / rétablir, piste précédente / suivante (comme les flèches de Cubase).
    undoButton.setButtonText (juce::String::fromUTF8 ("←"));
    redoButton.setButtonText (juce::String::fromUTF8 ("→"));
    prevButton.setButtonText (juce::String::fromUTF8 ("↑"));
    nextButton.setButtonText (juce::String::fromUTF8 ("↓"));
    undoButton.setTooltip ("Annuler (Cmd Z)");
    redoButton.setTooltip (juce::String::fromUTF8 ("Rétablir (Cmd Maj Z)"));
    prevButton.setTooltip (juce::String::fromUTF8 ("Piste précédente"));
    nextButton.setTooltip ("Piste suivante");
    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    prevButton.onClick = [this] { navigate (-1); };
    nextButton.onClick = [this] { navigate (1); };
    for (auto* b : { &undoButton, &redoButton, &prevButton, &nextButton })
        addAndMakeVisible (b);

    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd8c84a));
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff5aa9e6));
    muteButton.setTooltip ("Couper la voie");
    soloButton.setTooltip ("Solo");
    muteButton.onClick = [this]
    {
        const int t = target();
        engine.edit ([&] (Session& s) { if (juce::isPositiveAndBelow (t, s.song.numTracks())) s.song.tracks[(size_t) t].mute = ! s.song.tracks[(size_t) t].mute; return 0; });
    };
    soloButton.onClick = [this]
    {
        const int t = target();
        engine.edit ([&] (Session& s) { if (juce::isPositiveAndBelow (t, s.song.numTracks())) s.song.tracks[(size_t) t].solo = ! s.song.tracks[(size_t) t].solo; return 0; });
    };
    addAndMakeVisible (muteButton);
    addAndMakeVisible (soloButton);
    setWantsKeyboardFocus (true);

    setSize (960, 560);
    refresh();
    startTimerHz (20);
}

ChannelEditor::~ChannelEditor()
{
    stopTimer();
}

int ChannelEditor::target() const
{
    if (trackId == masterId)
        return AudioEngine::masterTarget;
    return engine.read ([this] (const Session& s)
    {
        for (int t = 0; t < s.song.numTracks(); ++t)
            if (s.song.tracks[(size_t) t].id == trackId)
                return t;
        return -2;
    });
}

juce::String ChannelEditor::getTitle() const
{
    return trackId == masterId ? juce::String ("Master") : name;
}

ChannelEq ChannelEditor::readEq() const
{
    const int t = target();
    return engine.read ([t] (const Session& s)
    {
        if (t == AudioEngine::masterTarget) return s.song.masterEq;
        return juce::isPositiveAndBelow (t, s.song.numTracks()) ? s.song.tracks[(size_t) t].eq : ChannelEq();
    });
}

void ChannelEditor::editEq (const std::function<void (ChannelEq&)>& change)
{
    remember();
    const int t = target();
    engine.edit ([&] (Session& s)
    {
        if (t == AudioEngine::masterTarget) change (s.song.masterEq);
        else if (juce::isPositiveAndBelow (t, s.song.numTracks())) change (s.song.tracks[(size_t) t].eq);
        return 0;
    });
    currentEq = readEq();
    for (int b = 0; b < 4; ++b)
        bands[b]->show (currentEq.bands[(size_t) b]);
    eqBypass.setToggleState (! currentEq.bypass, juce::dontSendNotification);
    graph->repaint();
}

void ChannelEditor::timerCallback()
{
    if (target() == -2)
    {
        if (onGone)
            onGone();
        return;
    }
    refresh();
}

void ChannelEditor::refresh()
{
    const int t = target();
    if (t == -2)
        return;

    struct Info
    {
        juce::String name;
        juce::Colour colour;
        ChannelEq eq;
        std::vector<InsertSlot> inserts;
        float volume = 0.85f, pan = 0.0f;
        bool mute = false, solo = false;
        int index = -1, count = 0;
    };
    const auto info = engine.read ([t] (const Session& s)
    {
        Info i;
        if (t == AudioEngine::masterTarget)
        {
            i.name = "Master";
            i.colour = theme::accent;
            i.eq = s.song.masterEq;
            i.volume = s.song.masterVolume;
            for (const auto& slot : s.song.masterInserts)
                i.inserts.push_back ({ slot.name, {}, {}, slot.bypass });
        }
        else
        {
            const auto& tr = s.song.tracks[(size_t) t];
            i.name = tr.name;
            i.colour = tr.colour;
            i.eq = tr.eq;
            i.volume = tr.volume;
            i.pan = tr.pan;
            i.mute = tr.mute;
            i.solo = tr.solo;
            i.index = t;
            i.count = s.song.numTracks();
            for (const auto& slot : tr.inserts)
                i.inserts.push_back ({ slot.name, {}, {}, slot.bypass });
        }
        return i;
    });

    if (name != info.name || colour != info.colour)
    {
        name = info.name;
        colour = info.colour;
        repaint (0, 0, getWidth(), 52);
    }
    if (currentEq != info.eq)
    {
        currentEq = info.eq;
        graph->repaint();
    }
    for (int b = 0; b < 4; ++b)
        bands[b]->show (currentEq.bands[(size_t) b]);
    eqBypass.setToggleState (! currentEq.bypass, juce::dontSendNotification);

    for (int i = 0; i < maxInserts; ++i)
    {
        const bool filled = i < (int) info.inserts.size() && info.inserts[(size_t) i].name.isNotEmpty();
        rows[i]->show (filled, filled ? info.inserts[(size_t) i].name : juce::String(), filled && info.inserts[(size_t) i].bypass);
    }

    if (! fader.isMouseButtonDown() && std::abs (fader.getValue() - info.volume) > 1.0e-4)
        fader.setValue (info.volume, juce::dontSendNotification);
    if (! pan.isMouseButtonDown() && std::abs (pan.getValue() - info.pan) > 1.0e-4)
        pan.setValue (info.pan, juce::dontSendNotification);
    const auto db = volumeToDecibels (info.volume);
    if (! faderValue.isBeingEdited())
        faderValue.setText (db <= -69.0f ? juce::String ("-oo") : juce::String (db, 2), juce::dontSendNotification);
    muteButton.setToggleState (info.mute, juce::dontSendNotification);
    soloButton.setToggleState (info.solo, juce::dontSendNotification);
    const bool isTrack = trackId != masterId;
    for (auto* c : std::initializer_list<juce::Component*> { &muteButton, &soloButton, &pan })
        c->setVisible (isTrack);
    prevButton.setEnabled (isTrack && info.index > 0);
    nextButton.setEnabled (isTrack && info.index >= 0 && info.index < info.count - 1);
    undoButton.setEnabled (! undoStack.empty());
    redoButton.setEnabled (! redoStack.empty());

    const auto meters = engine.getMeters();
    const float level = t == AudioEngine::masterTarget ? meters.back() : (juce::isPositiveAndBelow (t, (int) meters.size() - 1) ? meters[(size_t) t] : 0.0f);
    if (std::abs (level - meter) > 1.0e-4f)
    {
        meter = level;
        repaint (meterArea);
    }
    if (level > peakHold)
        peakHold = level;
    const float peakDb = juce::Decibels::gainToDecibels (peakHold, -100.0f);
    peakValue.setText (peakDb <= -99.0f ? juce::String ("-oo") : juce::String (peakDb, 1), juce::dontSendNotification);
    peakValue.setColour (juce::Label::textColourId, peakDb > -0.1f ? juce::Colour (0xffff4a3d) : theme::text);
}

void ChannelEditor::showInsertMenu (int slot, bool filled)
{
    // Les effets connus (pas les instruments), par ordre alphabétique.
    juce::Array<juce::PluginDescription> effects;
    for (const auto& d : engine.knownPlugins.getTypes())
        if (! d.isInstrument)
            effects.add (d);
    std::sort (effects.begin(), effects.end(), [] (const auto& a, const auto& b) { return a.name.compareIgnoreCase (b.name) < 0; });

    juce::PopupMenu list;
    for (int i = 0; i < effects.size(); ++i)
        list.addItem (100 + i, effects[i].name + "  (" + effects[i].pluginFormatName + ")");
    if (effects.isEmpty())
        list.addItem (-1, juce::String::fromUTF8 ("Aucun effet trouvé"), false);
    list.addSeparator();
    list.addItem (2, "Chercher des effets...");

    enum { up = 10, down, remove };
    juce::PopupMenu m;
    int count = 0;
    {
        const int t = target();
        count = engine.read ([t] (const Session& s)
        {
            if (t == AudioEngine::masterTarget) return (int) s.song.masterInserts.size();
            return juce::isPositiveAndBelow (t, s.song.numTracks()) ? (int) s.song.tracks[(size_t) t].inserts.size() : 0;
        });
    }
    if (filled)
    {
        m.addSubMenu ("Remplacer par", list);
        m.addItem (up, "Monter", slot > 0);
        m.addItem (down, "Descendre", slot < count - 1);
        m.addSeparator();
        m.addItem (remove, "Retirer l'effet");
    }
    else
        m = list;

    m.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ChannelEditor> (this), slot, effects] (int r)
    {
        auto* self = safe.getComponent();
        if (self == nullptr || r == 0)
            return;
        const int t = self->target();
        if (r == 2) { if (self->onScanPlugins) self->onScanPlugins(); }
        else if (r == up) { self->rememberMove (slot, slot - 1); self->engine.moveInsert (t, slot, slot - 1); }
        else if (r == down) { self->rememberMove (slot, slot + 1); self->engine.moveInsert (t, slot, slot + 1); }
        else if (r == remove) self->engine.removeInsert (t, slot);
        else if (r >= 100 && r - 100 < effects.size()) self->engine.addInsert (t, slot, effects.getReference (r - 100));
        self->refresh();
    });
}

//==============================================================================
// Annuler / rétablir, flèches de piste
ChannelEditor::Snapshot ChannelEditor::capture() const
{
    const int t = target();
    return engine.read ([t] (const Session& s)
    {
        Snapshot snap;
        const bool master = t == AudioEngine::masterTarget;
        if (! master && ! juce::isPositiveAndBelow (t, s.song.numTracks()))
            return snap;
        snap.eq = master ? s.song.masterEq : s.song.tracks[(size_t) t].eq;
        snap.volume = master ? s.song.masterVolume : s.song.tracks[(size_t) t].volume;
        snap.pan = master ? 0.0f : s.song.tracks[(size_t) t].pan;
        for (const auto& slot : master ? s.song.masterInserts : s.song.tracks[(size_t) t].inserts)
            snap.bypass.push_back (slot.bypass);
        return snap;
    });
}

void ChannelEditor::restore (const Snapshot& snap)
{
    const int t = target();
    engine.edit ([&] (Session& s)
    {
        const bool master = t == AudioEngine::masterTarget;
        if (! master && ! juce::isPositiveAndBelow (t, s.song.numTracks()))
            return 0;
        (master ? s.song.masterEq : s.song.tracks[(size_t) t].eq) = snap.eq;
        (master ? s.song.masterVolume : s.song.tracks[(size_t) t].volume) = snap.volume;
        if (! master)
            s.song.tracks[(size_t) t].pan = snap.pan;
        auto& slots = master ? s.song.masterInserts : s.song.tracks[(size_t) t].inserts;
        for (size_t i = 0; i < slots.size() && i < snap.bypass.size(); ++i)
            slots[i].bypass = snap.bypass[i];
        return 0;
    });
    refresh();
}

void ChannelEditor::remember()
{
    // Un geste continu (tirer un point, un fader) ne fait qu'une seule étape d'annulation.
    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastEdit > 600 || undoStack.empty())
    {
        undoStack.push_back (capture());
        if (undoStack.size() > 100)
            undoStack.erase (undoStack.begin());
        redoStack.clear();
    }
    lastEdit = now;
}

void ChannelEditor::rememberMove (int from, int to)
{
    Snapshot move;
    move.moveFrom = from;
    move.moveTo = to;
    undoStack.push_back (move);
    redoStack.clear();
    lastEdit = 0;
}

void ChannelEditor::undo()
{
    if (undoStack.empty())
        return;
    auto entry = undoStack.back();
    undoStack.pop_back();
    if (entry.moveFrom >= 0)
    {
        engine.moveInsert (target(), entry.moveTo, entry.moveFrom);
        redoStack.push_back (entry);
    }
    else
    {
        redoStack.push_back (capture());
        restore (entry);
    }
    lastEdit = 0;
    refresh();
}

void ChannelEditor::redo()
{
    if (redoStack.empty())
        return;
    auto entry = redoStack.back();
    redoStack.pop_back();
    if (entry.moveFrom >= 0)
    {
        engine.moveInsert (target(), entry.moveFrom, entry.moveTo);
        undoStack.push_back (entry);
    }
    else
    {
        undoStack.push_back (capture());
        restore (entry);
    }
    lastEdit = 0;
    refresh();
}

void ChannelEditor::navigate (int direction)
{
    if (trackId == masterId)
        return;
    const int t = target();
    const int id = engine.read ([t, direction] (const Session& s)
    {
        const int next = t + direction;
        return juce::isPositiveAndBelow (next, s.song.numTracks()) ? s.song.tracks[(size_t) next].id : -1;
    });
    if (id < 0)
        return;
    trackId = id;
    undoStack.clear();
    redoStack.clear();
    peakHold = 0.0f;
    refresh();
    repaint();
    if (onTrackChanged)
        onTrackChanged();
}

void ChannelEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == &peakValue)
        peakHold = 0.0f;   // clic sur la crête : remise à zéro
}

bool ChannelEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if ((mods.isCommandDown() || mods.isCtrlDown()) && (key.getKeyCode() == 'Z' || key.getTextCharacter() == 'z'))
    {
        if (mods.isShiftDown()) redo(); else undo();
        return true;
    }
    if (key == juce::KeyPress::upKey)   { navigate (-1); return true; }
    if (key == juce::KeyPress::downKey) { navigate (1); return true; }
    return false;
}

//==============================================================================
void ChannelEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    // En-tête façon plugin : bandeau de couleur, nom en italique, filet.
    g.setGradientFill (juce::ColourGradient (theme::panel.brighter (0.05f), 0.0f, 0.0f, theme::panel, 0.0f, 52.0f, false));
    g.fillRect (0, 0, getWidth(), 52);
    g.setColour (colour);
    g.fillRect (0, 0, getWidth(), 4);
    g.setColour (theme::text);
    g.setFont (theme::font (24.0f).italicised());
    g.drawText (getTitle(), 18, 8, 400, 34, juce::Justification::centredLeft);
    theme::sectionTitle (g, juce::String::fromUTF8 ("RÉGLAGES DE VOIE"), { getWidth() - 470, 8, 200, 34 }, juce::Justification::centredRight);
    g.setColour (theme::rule);
    g.fillRect (14, 51, getWidth() - 28, 1);

    for (auto area : { insertsArea, eqArea, channelArea })
    {
        g.setColour (theme::panel);
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
    }
    theme::sectionTitle (g, "INSERTS", insertsArea.withHeight (28).reduced (12, 0));
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.5f));
    g.drawFittedText (juce::String::fromUTF8 ("Le son traverse les effets de 1 à 8 : l'ordre compte. Glisser un effet pour le déplacer."),
                      insertsArea.reduced (12, 0).removeFromBottom (40), juce::Justification::centredLeft, 2, 1.0f);
    theme::sectionTitle (g, juce::String::fromUTF8 ("ÉGALISEUR"), eqArea.withHeight (28).reduced (12, 0));
    theme::sectionTitle (g, "VOIE", channelArea.withHeight (28).reduced (12, 0));

    // Graduation du fader, comme sur une voie de Cubase (en dB sous 0).
    g.setFont (theme::font (10.5f, juce::Font::bold));
    for (float db : { 0.0f, -5.0f, -10.0f, -15.0f, -20.0f, -30.0f, -40.0f, -50.0f })
    {
        const double v = 0.85 * std::sqrt (juce::Decibels::decibelsToGain ((double) db));
        const float y = (float) fader.getY() + (float) fader.getPositionOfValue (v);
        g.setColour (theme::textDim);
        g.drawText (juce::String ((int) std::abs (db)), scaleArea.getX(), (int) y - 6, scaleArea.getWidth() - 8, 12, juce::Justification::centredRight);
        g.setColour (juce::Colour (0xff4a4640));
        g.fillRect ((float) scaleArea.getRight() - 6.0f, y, 5.0f, 1.0f);
    }
    g.setColour (theme::textDim);
    g.drawText ("oo", scaleArea.getX(), (int) (fader.getY() + fader.getPositionOfValue (0.0)) - 6, scaleArea.getWidth() - 8, 12, juce::Justification::centredRight);

    // Vu-mètre gradué : vert, puis jaune et rouge près de 0 dB.
    const auto m = meterArea.toFloat();
    g.setColour (juce::Colour (0xff0b0b0b));
    g.fillRoundedRectangle (m, 2.0f);
    const float level = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (meter, -60.0f) + 60.0f) / 66.0f);
    const auto filled = m.withTop (m.getBottom() - m.getHeight() * level).reduced (1.5f, 0.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff4a3d), 0.0f, m.getY(), theme::play, 0.0f, m.getBottom(), false));
    g.fillRect (filled);
    for (int db : { 0, 6, 12, 18, 24, 30, 40, 50, 60 })
    {
        const float y = m.getBottom() - m.getHeight() * (60.0f - (float) db) / 66.0f;
        g.setColour (juce::Colour (0xff4a4640));
        g.fillRect (m.getRight() + 2.0f, y, 4.0f, 1.0f);
        g.setColour (theme::textDim);
        g.drawText (juce::String (db), (int) m.getRight() + 7, (int) y - 6, 22, 12, juce::Justification::centredLeft);
    }
}

void ChannelEditor::resized()
{
    auto r = getLocalBounds().withTrimmedTop (60).reduced (12, 0).withTrimmedBottom (12);
    insertsArea = r.removeFromLeft (240);
    r.removeFromLeft (10);
    channelArea = r.removeFromRight (176);
    r.removeFromRight (10);
    eqArea = r;

    auto ins = insertsArea.reduced (8, 0).withTrimmedTop (30).withTrimmedBottom (44);
    for (auto* row : rows)
    {
        row->setBounds (ins.removeFromTop (30));
        ins.removeFromTop (4);
    }

    auto eqr = eqArea.reduced (10, 0).withTrimmedTop (30).withTrimmedBottom (10);
    eqBypass.setBounds (eqArea.getRight() - 150, eqArea.getY() + 4, 44, 22);
    eqReset.setBounds (eqArea.getRight() - 100, eqArea.getY() + 4, 90, 22);
    graph->setBounds (eqr.removeFromTop (eqr.getHeight() - 130));
    eqr.removeFromTop (10);
    const int w = eqr.getWidth() / 4;
    for (auto* b : bands)
        b->setBounds (eqr.removeFromLeft (w).reduced (4, 0));

    // En-tête : flèches annuler / rétablir et piste précédente / suivante.
    auto head = juce::Rectangle<int> (getWidth() - 250, 12, 236, 28);
    nextButton.setBounds (head.removeFromRight (34));
    head.removeFromRight (2);
    prevButton.setBounds (head.removeFromRight (34));
    head.removeFromRight (12);
    redoButton.setBounds (head.removeFromRight (34));
    head.removeFromRight (2);
    undoButton.setBounds (head.removeFromRight (34));

    auto ch = channelArea.reduced (10, 0).withTrimmedTop (30).withTrimmedBottom (10);
    auto ms = ch.removeFromTop (24);
    muteButton.setBounds (ms.removeFromLeft (ms.getWidth() / 2 - 2));
    ms.removeFromLeft (4);
    soloButton.setBounds (ms);
    ch.removeFromTop (6);
    pan.setBounds (ch.removeFromTop (20));
    ch.removeFromTop (10);
    auto values = ch.removeFromBottom (22);
    faderValue.setBounds (values.removeFromLeft (values.getWidth() / 2 - 2));
    values.removeFromLeft (4);
    peakValue.setBounds (values);
    ch.removeFromBottom (8);
    scaleArea = ch.removeFromLeft (30);
    fader.setBounds (ch.removeFromLeft (40));
    ch.removeFromLeft (10);
    meterArea = ch.removeFromLeft (12);
}
} // namespace gridplay
