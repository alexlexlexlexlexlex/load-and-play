#include "NoteEditor.h"
#include "TransportControls.h"

namespace gridplay
{
namespace
{
    bool isBlackKey (int pitch)
    {
        const int k = pitch % 12;
        return k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
    }

    std::vector<Note> clipboard;   // notes copiées, au début de la première

    // Commandes de la boîte à outils (les outils ont l'identifiant 1000 + outil).
    enum Command { quantizeCmd = 1, quantizeLengthsCmd, legatoCmd, muteCmd, unmuteCmd, duplicateCmd, deleteCmd, selectAllCmd, toolBase = 1000 };

    const NoteEditor::Tool toolOrder[] = { NoteEditor::Tool::select, NoteEditor::Tool::draw, NoteEditor::Tool::erase, NoteEditor::Tool::split,
                                           NoteEditor::Tool::glue, NoteEditor::Tool::mute, NoteEditor::Tool::zoom };
}

//==============================================================================
juce::String NoteEditor::toolName (Tool t)
{
    switch (t)
    {
        case Tool::select: return juce::String::fromUTF8 ("Sélection (1) : sélectionner, déplacer, changer la longueur");
        case Tool::split:  return "Ciseaux (3) : couper une note en deux";
        case Tool::glue:   return juce::String::fromUTF8 ("Colle (4) : joindre une note à la suivante de même hauteur");
        case Tool::erase:  return "Gomme (5) : effacer des notes";
        case Tool::zoom:   return juce::String::fromUTF8 ("Zoom (6) : clic pour agrandir, Alt-clic pour réduire");
        case Tool::mute:   return "Sourdine (7) : la note reste mais ne joue plus";
        case Tool::draw:   return "Crayon (8) : dessiner des notes (clic sur une note : l'effacer)";
    }
    return {};
}

void NoteEditor::drawToolIcon (juce::Graphics& g, Tool tool, juce::Rectangle<float> r, juce::Colour c)
{
    g.setColour (c);
    juce::Path p;
    switch (tool)
    {
        case Tool::select:   // flèche
            p.startNewSubPath (r.getX() + 2, r.getY());
            p.lineTo (r.getX() + 2, r.getBottom() - 2);
            p.lineTo (r.getX() + 5.5f, r.getBottom() - 5.5f);
            p.lineTo (r.getX() + 8, r.getBottom());
            p.lineTo (r.getX() + 10, r.getBottom() - 1);
            p.lineTo (r.getX() + 7.5f, r.getBottom() - 6);
            p.lineTo (r.getRight() - 1, r.getBottom() - 6);
            p.closeSubPath();
            g.fillPath (p);
            break;
        case Tool::draw:     // crayon
            p.addRectangle (-2.0f, -7.0f, 4.0f, 11.0f);
            p.addTriangle (-2.0f, 4.5f, 2.0f, 4.5f, 0.0f, 8.0f);
            g.fillPath (p, juce::AffineTransform::rotation (0.75f).translated (r.getCentreX(), r.getCentreY()));
            break;
        case Tool::erase:    // gomme
            p.addRoundedRectangle (-6.0f, -3.5f, 12.0f, 7.0f, 1.5f);
            g.fillPath (p, juce::AffineTransform::rotation (-0.6f).translated (r.getCentreX(), r.getCentreY()));
            break;
        case Tool::split:    // ciseaux
            g.drawEllipse (r.getX(), r.getBottom() - 5, 5, 5, 1.5f);
            g.drawEllipse (r.getRight() - 5, r.getBottom() - 5, 5, 5, 1.5f);
            g.drawLine (r.getX() + 4, r.getBottom() - 4, r.getRight() - 2, r.getY(), 1.6f);
            g.drawLine (r.getRight() - 4, r.getBottom() - 4, r.getX() + 2, r.getY(), 1.6f);
            break;
        case Tool::glue:     // tube de colle
            p.addRoundedRectangle (r.getX(), r.getY() + 5, 9.0f, 6.0f, 1.5f);
            p.addTriangle (r.getX() + 9, r.getY() + 5.5f, r.getX() + 9, r.getY() + 10.5f, r.getX() + 12.5f, r.getY() + 8);
            g.fillPath (p);
            g.fillEllipse (r.getRight() - 2.5f, r.getY() + 10.5f, 2.5f, 3.5f);
            break;
        case Tool::mute:     // note barrée
            g.drawRect (r.reduced (0, 3.5f), 1.5f);
            g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.6f);
            break;
        case Tool::zoom:     // loupe
            g.drawEllipse (r.getX(), r.getY(), 9, 9, 1.6f);
            g.drawLine (r.getX() + 8, r.getY() + 8, r.getRight(), r.getBottom(), 2.0f);
            break;
    }
}

//==============================================================================
// Bouton d'outil.
class NoteEditor::ToolButton : public juce::Button
{
public:
    ToolButton (Tool t) : juce::Button (toolName (t)), tool (t)
    {
        setTooltip (toolName (t));
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, findColour (juce::TextButton::buttonColourId), over, down);
        drawToolIcon (g, tool, getLocalBounds().toFloat().withSizeKeepingCentre (14.0f, 14.0f),
                      getToggleState() ? juce::Colours::black : theme::text);
    }

    Tool tool;
};

//==============================================================================
// Champ de la ligne d'infos : clic pour taper, molette pour changer, comme dans Cubase.
class NoteEditor::InfoField : public juce::Label
{
public:
    std::function<void (const juce::String&)> onCommit;
    std::function<void (int)> onStep;

    InfoField()
    {
        setEditable (true, true, false);
        setJustificationType (juce::Justification::centredLeft);
        setColour (juce::Label::backgroundColourId, juce::Colour (0xff0d0d0d));
        setColour (juce::Label::outlineColourId, theme::rule);
        setColour (juce::Label::textColourId, theme::text);
        setFont (theme::font (13.0f, juce::Font::bold));
        setMouseCursor (juce::MouseCursor::IBeamCursor);
        onTextChange = [this] { if (onCommit) onCommit (getText()); };
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (onStep && std::abs (w.deltaY) > 1.0e-4f && isEnabled())
            onStep (w.deltaY > 0 ? 1 : -1);
    }

    void show (const juce::String& text)
    {
        if (! isBeingEdited())
            setText (text, juce::dontSendNotification);
    }
};

//==============================================================================
// Boîte à outils du clic droit (comme Cubase) : on peut garder le bouton appuyé,
// glisser sur un outil ou une commande et relâcher ; ou cliquer, puis choisir.
class NoteEditor::Toolbox : public juce::Component
{
public:
    explicit Toolbox (NoteEditor& o) : owner (o)
    {
        const bool any = ! owner.selection.empty();
        items = { { quantizeCmd, juce::String::fromUTF8 ("Quantifier"), "Q", true },
                  { quantizeLengthsCmd, "Quantifier les longueurs", {}, true },
                  { legatoCmd, "Legato", {}, true },
                  { muteCmd, "Mettre en sourdine", {}, any },
                  { unmuteCmd, "Retirer la sourdine", {}, any },
                  { duplicateCmd, "Dupliquer", "Cmd D", any },
                  { deleteCmd, "Supprimer", "Suppr", any },
                  { selectAllCmd, juce::String::fromUTF8 ("Tout sélectionner"), "Cmd A", true } };
        setSize (pad * 2 + 7 * cell, pad * 2 + cell + labelH + 6 + (int) items.size() * rowH);
        setWantsKeyboardFocus (false);
    }

    int itemAt (juce::Point<int> p) const
    {
        if (! getLocalBounds().contains (p))
            return 0;
        if (p.y >= pad && p.y < pad + cell)
        {
            const int i = (p.x - pad) / cell;
            return juce::isPositiveAndBelow (i, 7) ? toolBase + (int) toolOrder[i] : 0;
        }
        const int top = pad + cell + labelH + 6;
        const int r = (p.y - top) / rowH;
        if (p.y >= top && juce::isPositiveAndBelow (r, (int) items.size()) && items[(size_t) r].enabled)
            return items[(size_t) r].id;
        return 0;
    }

    void setHover (int id)
    {
        if (hover != id)
        {
            hover = id;
            repaint();
        }
    }

    int getHover() const { return hover; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (0, 2), 6.0f);
        g.setColour (theme::panel);
        g.fillRoundedRectangle (r.reduced (0, 1), 6.0f);
        g.setColour (theme::rule);
        g.drawRoundedRectangle (r.reduced (0.5f, 1.5f), 6.0f, 1.0f);

        for (int i = 0; i < 7; ++i)
        {
            const auto t = toolOrder[i];
            const auto box = juce::Rectangle<float> ((float) (pad + i * cell), (float) pad, (float) cell, (float) cell).reduced (2.0f);
            const bool current = owner.tool == t, hovered = hover == toolBase + (int) t;
            g.setColour (current ? theme::accent : hovered ? theme::panelLight.brighter (0.15f) : theme::panelLight);
            g.fillRoundedRectangle (box, 4.0f);
            drawToolIcon (g, t, box.withSizeKeepingCentre (14.0f, 14.0f), current ? juce::Colours::black : theme::text);
        }

        // Nom de l'outil survolé (ou de l'outil courant).
        auto label = juce::Rectangle<int> (pad, pad + cell, getWidth() - 2 * pad, labelH);
        const auto shown = hover >= toolBase ? (Tool) (hover - toolBase) : owner.tool;
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.0f));
        g.drawText (toolName (shown), label, juce::Justification::centredLeft, true);

        int y = pad + cell + labelH + 6;
        g.setColour (theme::rule);
        g.fillRect (pad, y - 4, getWidth() - 2 * pad, 1);
        for (const auto& item : items)
        {
            const auto row = juce::Rectangle<int> (4, y, getWidth() - 8, rowH);
            if (hover == item.id)
            {
                g.setColour (theme::accent);
                g.fillRoundedRectangle (row.toFloat(), 3.0f);
            }
            g.setColour (! item.enabled ? theme::textDim.withAlpha (0.5f) : hover == item.id ? juce::Colours::black : theme::text);
            g.setFont (theme::font (13.0f));
            g.drawText (item.name, row.withTrimmedLeft (8), juce::Justification::centredLeft);
            g.setColour (hover == item.id ? juce::Colours::black.withAlpha (0.6f) : theme::textDim);
            g.setFont (theme::font (11.0f));
            g.drawText (item.key, row.withTrimmedRight (8), juce::Justification::centredRight);
            y += rowH;
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { setHover (itemAt (e.getPosition())); }
    void mouseExit (const juce::MouseEvent&) override   { setHover (0); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int id = itemAt (e.getPosition());
        if (id != 0)
            owner.runCommand (id);
    }

private:
    struct Item { int id; juce::String name, key; bool enabled; };
    static constexpr int pad = 8, cell = 34, labelH = 18, rowH = 22;
    NoteEditor& owner;
    std::vector<Item> items;
    int hover = 0;
};

//==============================================================================
// Panneau de quantification (comme celui de Cubase) : swing, force, fins de notes.
class NoteEditor::QuantizePanel : public juce::Component
{
public:
    explicit QuantizePanel (NoteEditor& o) : owner (&o)
    {
        for (auto* s : { &swing, &strength })
        {
            s->setSliderStyle (juce::Slider::LinearHorizontal);
            s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
            s->setRange (0.0, 100.0, 1.0);
            s->textFromValueFunction = [] (double v) { return juce::String ((int) v) + " %"; };
            addAndMakeVisible (s);
        }
        swing.setValue (o.quantizeSettings.swing * 100.0, juce::dontSendNotification);
        strength.setValue (o.quantizeSettings.strength * 100.0, juce::dontSendNotification);
        swing.setTooltip ("Retarde une note sur deux de la grille (groove)");
        strength.setTooltip (juce::String::fromUTF8 ("100 % : colle à la grille ; moins : rapproche seulement, le jeu reste humain"));
        swing.onValueChange = [this] { if (owner != nullptr) owner->quantizeSettings.swing = swing.getValue() / 100.0; };
        strength.onValueChange = [this] { if (owner != nullptr) owner->quantizeSettings.strength = strength.getValue() / 100.0; };

        lengths.setButtonText ("Quantifier aussi les fins de notes");
        lengths.setToggleState (o.quantizeSettings.lengths, juce::dontSendNotification);
        lengths.onClick = [this] { if (owner != nullptr) owner->quantizeSettings.lengths = lengths.getToggleState(); };
        addAndMakeVisible (lengths);

        apply.setButtonText ("Quantifier (Q)");
        apply.setColour (juce::TextButton::buttonColourId, theme::accent);
        apply.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        apply.onClick = [this] { if (owner != nullptr) owner->quantize(); };
        addAndMakeVisible (apply);
        setSize (320, 156);
    }

    void paint (juce::Graphics& g) override
    {
        theme::sectionTitle (g, "QUANTIFICATION", { 12, 8, 200, 16 });
        g.setColour (theme::text);
        g.setFont (theme::font (12.0f));
        g.drawText ("Swing", 12, 32, 70, 22, juce::Justification::centredLeft);
        g.drawText ("Force", 12, 60, 70, 22, juce::Justification::centredLeft);
    }

    void resized() override
    {
        swing.setBounds (80, 32, 228, 22);
        strength.setBounds (80, 60, 228, 22);
        lengths.setBounds (8, 88, 300, 22);
        apply.setBounds (12, 118, 296, 28);
    }

private:
    juce::Component::SafePointer<NoteEditor> owner;
    juce::Slider swing, strength;
    juce::ToggleButton lengths;
    juce::TextButton apply;
};

//==============================================================================
NoteEditor::NoteEditor (AudioEngine& e, ClipPtr c) : engine (e)
{
    setWantsKeyboardFocus (true);
    rowForPitch.fill (-1);

    // Transport de l'éditeur : depuis le curseur, comme Cubase.
    playButton.setTooltip ("Lire ce clip en boucle depuis le curseur (barre d'espace)");
    stopButton.setTooltip (juce::String::fromUTF8 ("Arrêter (le curseur reste où il est ; un second stop le ramène au début)"));
    playButton.setColour (juce::TextButton::buttonOnColourId, theme::play);
    playButton.onClick = [this] { playFromCursor(); grabKeyboardFocus(); };
    stopButton.onClick = [this] { stopPlayback(); grabKeyboardFocus(); };

    for (auto t : toolOrder)
    {
        auto* b = tools.add (new ToolButton (t));
        b->setColour (juce::TextButton::buttonOnColourId, theme::accent);
        b->onClick = [this, chosen = t] { setTool (chosen); grabKeyboardFocus(); };
        addAndMakeVisible (b);
    }

    snapButton.setTooltip ("Calage sur la grille de quantification (J)");
    snapButton.onClick = [this] { snap = ! snap; updateToolbar(); grabKeyboardFocus(); };
    for (size_t i = 0; i < notes::gridValues().size(); ++i)
    {
        gridBox.addItem (notes::gridValues()[i].name, (int) i + 1);
        lengthBox.addItem (notes::gridValues()[i].name, (int) i + 2);
    }
    lengthBox.addItem ("Quantif.", 1);
    gridBox.setTooltip (juce::String::fromUTF8 ("Quantification : sert au calage et à la touche Q (comme dans Cubase)"));
    lengthBox.setTooltip (juce::String::fromUTF8 ("Longueur des notes dessinées au crayon"));
    gridBox.onChange = [this] { gridIndex = juce::jmax (0, gridBox.getSelectedId() - 1); repaint(); grabKeyboardFocus(); };
    lengthBox.onChange = [this] { lengthIndex = juce::jmax (0, lengthBox.getSelectedId() - 1); grabKeyboardFocus(); };

    quantizeButton.setTooltip (juce::String::fromUTF8 ("Quantifier les notes sélectionnées, ou toutes (Q)"));
    quantizeButton.onClick = [this] { quantize(); grabKeyboardFocus(); };
    quantizeMore.setTooltip (juce::String::fromUTF8 ("Panneau de quantification : swing, force, fins de notes"));
    quantizeMore.onClick = [this] { showQuantizePanel(); };

    listenButton.setButtonText (juce::String::fromUTF8 ("Écoute"));
    listenButton.setTooltip (juce::String::fromUTF8 ("Entendre les notes quand on les pose, les déplace ou clique le clavier"));
    listenButton.onClick = [this] { listen = ! listen; updateToolbar(); grabKeyboardFocus(); };

    drumButton.setTooltip (juce::String::fromUTF8 ("Drum map : les noms des sons de batterie (General MIDI) au lieu du clavier, notes en losanges. "
                                                   "Se règle par piste."));
    drumButton.onClick = [this]
    {
        const bool now = ! drumMode;
        if (location.track >= 0)
            engine.edit ([&] (Session& s) { s.song.tracks[(size_t) location.track].drumMap = now; return 0; });
        location.drumMap = now;
        drumMode = now;
        rebuildRows();
        resized();
        scrollToNotes();
        updateToolbar();
        repaint();
        grabKeyboardFocus();
    };

    for (auto* l : { &lengthLabel, &velocityLabel, &loopLabel })
    {
        l->setFont (theme::font (10.5f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, theme::textDim);
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    }
    lengthLabel.setText ("LONGUEUR", juce::dontSendNotification);
    velocityLabel.setText (juce::String::fromUTF8 ("VÉL."), juce::dontSendNotification);
    loopLabel.setText ("BOUCLE", juce::dontSendNotification);

    velocityField = std::make_unique<InfoField>();
    velocityField->setTooltip (juce::String::fromUTF8 ("Vélocité des notes dessinées (molette pour changer)"));
    velocityField->onCommit = [this] (const juce::String& t)
    {
        if (t.getIntValue() > 0) insertVelocity = juce::jlimit (1, 127, t.getIntValue());
        updateToolbar();
    };
    velocityField->onStep = [this] (int d) { insertVelocity = juce::jlimit (1, 127, insertVelocity + d); updateToolbar(); };

    loopField = std::make_unique<InfoField>();
    loopField->setTooltip (juce::String::fromUTF8 ("Longueur de la boucle en mesures.temps (molette, ou glisser le triangle orange de la règle)"));
    loopField->onCommit = [this] (const juce::String& t)
    {
        const double beats = notes::parsePosition (t, true);
        if (beats > 0.0) setLoopLength (beats);
        updateToolbar();
    };
    loopField->onStep = [this] (int d) { setLoopLength (length + d * (double) beatsPerBar); };
    loopMinus.setRepeatSpeed (350, 90);
    loopPlus.setRepeatSpeed (350, 90);
    loopMinus.onClick = [this] { setLoopLength (length - (juce::ModifierKeys::currentModifiers.isShiftDown() ? 1.0 : (double) beatsPerBar)); };
    loopPlus.onClick = [this] { setLoopLength (length + (juce::ModifierKeys::currentModifiers.isShiftDown() ? 1.0 : (double) beatsPerBar)); };
    loopMinus.setTooltip ("Boucle : une mesure de moins (Maj : un temps)");
    loopPlus.setTooltip ("Boucle : une mesure de plus (Maj : un temps)");

    title.setFont (theme::font (17.0f, juce::Font::italic));
    title.setColour (juce::Label::textColourId, theme::text);
    title.setJustificationType (juce::Justification::centredRight);

    for (auto* child : std::initializer_list<juce::Component*> { &playButton, &stopButton, &snapButton, &gridBox, &lengthBox, &quantizeButton,
                                                                 &quantizeMore, velocityField.get(), loopField.get(), &loopMinus, &loopPlus,
                                                                 &listenButton, &drumButton, &title, &hBar, &vBar })
        addAndMakeVisible (child);

    // Ligne d'infos
    const char* names[] = { "DÉBUT", "FIN", "LONGUEUR", "HAUTEUR", "VÉLOCITÉ" };
    const char* tips[] = { "Début de la note (mesure.temps.double-croche.tick) : cliquer pour taper, molette pour décaler d'un pas",
                           "Fin de la note", "Longueur de la note", "Hauteur (C3 = 60, comme Cubase)", "Vélocité (1 à 127)" };
    for (int i = 0; i < 5; ++i)
    {
        auto* label = infoLabels.add (new juce::Label ({}, juce::String::fromUTF8 (names[i])));
        label->setFont (theme::font (10.5f, juce::Font::bold));
        label->setColour (juce::Label::textColourId, theme::textDim);
        label->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (label);
        auto* field = info.add (new InfoField());
        field->setTooltip (juce::String::fromUTF8 (tips[i]) + juce::String::fromUTF8 (" — s'applique à toutes les notes sélectionnées"));
        addAndMakeVisible (field);
    }
    info[0]->onCommit = [this] (const juce::String& t)
    {
        const double at = notes::parsePosition (t, false);
        if (at >= 0.0 && ! selection.empty())
            commit (true, [&] { notes::nudge (notes, selection, at - notes[(size_t) firstSelected()].start); });
        updateInfo();
    };
    info[1]->onCommit = [this] (const juce::String& t)
    {
        const double end = notes::parsePosition (t, false);
        if (end >= 0.0 && ! selection.empty())
        {
            const auto& n = notes[(size_t) firstSelected()];
            commit (true, [&, delta = end - (n.start + n.length)] { notes::resize (notes, selection, delta, 1.0 / 64.0); });
        }
        updateInfo();
    };
    info[2]->onCommit = [this] (const juce::String& t)
    {
        const double len = notes::parsePosition (t, true);
        if (len > 0.0 && ! selection.empty())
            commit (true, [&] { for (int i : selection) notes[(size_t) i].length = len; });
        updateInfo();
    };
    info[3]->onCommit = [this] (const juce::String& t)
    {
        const int pitch = notes::parsePitch (t);
        if (pitch >= 0 && ! selection.empty())
        {
            const int delta = pitch - notes[(size_t) firstSelected()].pitch;
            commit (true, [&] { notes::transpose (notes, selection, delta); });
        }
        updateInfo();
    };
    info[4]->onCommit = [this] (const juce::String& t)
    {
        const int v = t.getIntValue();
        if (v > 0 && ! selection.empty())
        {
            const int delta = juce::jlimit (1, 127, v) - notes[(size_t) firstSelected()].velocity;
            commit (true, [&] { for (int i : selection) notes[(size_t) i].velocity = juce::jlimit (1, 127, notes[(size_t) i].velocity + delta); });
        }
        updateInfo();
    };
    info[0]->onStep = [this] (int d) { nudgeSelection (d * grid()); };
    info[1]->onStep = [this] (int d) { commit (true, [&] { notes::resize (notes, selection, d * grid(), 1.0 / 64.0); }); };
    info[2]->onStep = [this] (int d) { commit (true, [&] { notes::resize (notes, selection, d * grid(), 1.0 / 64.0); }); };
    info[3]->onStep = [this] (int d) { if (! selection.empty()) commit (true, [&] { notes::transpose (notes, selection, d); }); };
    info[4]->onStep = [this] (int d) { commit (true, [&] { for (int i : selection) notes[(size_t) i].velocity = juce::jlimit (1, 127, notes[(size_t) i].velocity + d); }); };

    hBar.addListener (this);
    vBar.addListener (this);
    hBar.setAutoHide (false);
    vBar.setAutoHide (false);

    setClip (std::move (c));
    setTool (Tool::select);
    updateToolbar();
    startTimerHz (30);
}

NoteEditor::~NoteEditor()
{
    stopTimer();
    stopPreview();
}

void NoteEditor::setClip (ClipPtr c)
{
    stopPreview();
    closeToolbox();
    clip = std::move (c);
    selection.clear();
    undoStack.clear();
    redoStack.clear();
    drag = Drag::none;
    cursorBeat = 0.0;
    if (clip != nullptr)
        engine.read ([this] (const Session&) { notes = clip->notes; length = clip->length; return 0; });
    location = locate();
    drumMode = location.drumMap;
    rebuildRows();
    viewInitialised = false;
    if (getWidth() > 0)
        resized();
    updateInfo();
    updateToolbar();
    repaint();
}

void NoteEditor::setTool (Tool t)
{
    tool = t;
    for (auto* b : tools)
        b->setToggleState (b->tool == t, juce::dontSendNotification);
    switch (t)
    {
        case Tool::draw:   setMouseCursor (juce::MouseCursor::CrosshairCursor); break;
        case Tool::erase:  setMouseCursor (juce::MouseCursor::CrosshairCursor); break;
        case Tool::split:  setMouseCursor (juce::MouseCursor::IBeamCursor); break;
        case Tool::zoom:   setMouseCursor (juce::MouseCursor::PointingHandCursor); break;
        case Tool::select:
        case Tool::glue:
        case Tool::mute:   setMouseCursor (juce::MouseCursor::NormalCursor); break;
    }
}

//==============================================================================
// Lignes : piano (127 en haut) ou drum map
void NoteEditor::rebuildRows()
{
    rows.clear();
    if (drumMode)
        rows = notes::drumRows (notes);
    else
        for (int p = 127; p >= 0; --p)
            rows.push_back (p);
    rowForPitch.fill (-1);
    for (int r = 0; r < (int) rows.size(); ++r)
        rowForPitch[(size_t) rows[(size_t) r]] = r;
}

int NoteEditor::rowOf (int pitch) const        { return juce::isPositiveAndBelow (pitch, 128) ? rowForPitch[(size_t) pitch] : -1; }
int NoteEditor::pitchAtRow (int row) const     { return rows[(size_t) juce::jlimit (0, numRows() - 1, row)]; }

//==============================================================================
// Modifications et annulation
void NoteEditor::pushUndo()
{
    undoStack.emplace_back (notes, length);
    if (undoStack.size() > 200)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void NoteEditor::sendToClip()
{
    if (clip == nullptr)
        return;
    engine.edit ([this] (Session&) { clip->notes = notes; clip->length = length; return 0; });
}

void NoteEditor::commit (bool undoable, const std::function<void()>& change)
{
    if (clip == nullptr)
        return;
    if (undoable)
        pushUndo();
    change();
    for (auto it = selection.begin(); it != selection.end();)
        it = *it >= (int) notes.size() ? selection.erase (it) : std::next (it);
    if (drumMode)
        rebuildRows();
    sendToClip();
    updateInfo();
    repaint();
}

void NoteEditor::undo()
{
    if (undoStack.empty())
        return;
    redoStack.emplace_back (notes, length);
    std::tie (notes, length) = undoStack.back();
    undoStack.pop_back();
    selection.clear();
    if (drumMode)
        rebuildRows();
    sendToClip();
    updateInfo();
    updateToolbar();
    repaint();
}

void NoteEditor::redo()
{
    if (redoStack.empty())
        return;
    undoStack.emplace_back (notes, length);
    std::tie (notes, length) = redoStack.back();
    redoStack.pop_back();
    selection.clear();
    if (drumMode)
        rebuildRows();
    sendToClip();
    updateInfo();
    updateToolbar();
    repaint();
}

//==============================================================================
// Coordonnées
double NoteEditor::grid() const          { return notes::gridValues()[(size_t) gridIndex].beats; }
double NoteEditor::insertLength() const  { return lengthIndex == 0 ? grid() : notes::gridValues()[(size_t) (lengthIndex - 1)].beats; }

double NoteEditor::beatAtX (float x) const    { return (x - gridArea.getX() + scrollX) / pixelsPerBeat; }
float NoteEditor::xForBeat (double beat) const { return (float) (gridArea.getX() + beat * pixelsPerBeat - scrollX); }
int NoteEditor::rowAtY (float y) const
{
    return juce::jlimit (0, numRows() - 1, (int) std::floor ((y - gridArea.getY() + scrollY) / rowHeight));
}
int NoteEditor::pitchAtY (float y) const   { return pitchAtRow (rowAtY (y)); }
float NoteEditor::yForPitch (int pitch) const
{
    const int r = rowOf (pitch);
    return r < 0 ? -100000.0f : (float) (gridArea.getY() + r * rowHeight - scrollY);
}

juce::Rectangle<float> NoteEditor::noteBounds (const Note& n) const
{
    const float x = xForBeat (n.start), y = yForPitch (n.pitch);
    if (drumMode)   // losange centré sur le début de la note
        return { x - rowHeight * 0.5f, y + 1.0f, rowHeight, rowHeight - 1.0f };
    return { x, y + 1.0f, juce::jmax (3.0f, (float) (n.length * pixelsPerBeat) - 1.0f), rowHeight - 1.0f };
}

int NoteEditor::noteAt (juce::Point<float> p) const
{
    for (int i = (int) notes.size(); --i >= 0;)
        if (noteBounds (notes[(size_t) i]).expanded (0.0f, 0.5f).contains (p))
            return i;
    return -1;
}

double NoteEditor::contentBeats() const
{
    double end = length;
    for (const auto& n : notes)
        end = juce::jmax (end, n.start + n.length);
    return std::ceil (end / beatsPerBar) * beatsPerBar + 2.0 * beatsPerBar;
}

void NoteEditor::updateScrollBars()
{
    const double contentW = contentBeats() * pixelsPerBeat;
    const double contentH = numRows() * rowHeight;
    scrollX = juce::jlimit (0.0, juce::jmax (0.0, contentW - gridArea.getWidth()), scrollX);
    scrollY = juce::jlimit (0.0, juce::jmax (0.0, contentH - gridArea.getHeight()), scrollY);
    hBar.setRangeLimits (0.0, juce::jmax (contentW, (double) gridArea.getWidth()), juce::dontSendNotification);
    hBar.setCurrentRange (scrollX, gridArea.getWidth(), juce::dontSendNotification);
    vBar.setRangeLimits (0.0, juce::jmax (contentH, (double) gridArea.getHeight()), juce::dontSendNotification);
    vBar.setCurrentRange (scrollY, gridArea.getHeight(), juce::dontSendNotification);
}

void NoteEditor::scrollBarMoved (juce::ScrollBar* bar, double start)
{
    (bar == &hBar ? scrollX : scrollY) = start;
    repaint();
}

void NoteEditor::zoomHorizontal (double factor, float anchorX)
{
    const double anchorBeat = beatAtX (anchorX);
    pixelsPerBeat = juce::jlimit (12.0, 600.0, pixelsPerBeat * factor);
    scrollX = anchorBeat * pixelsPerBeat - (anchorX - gridArea.getX());
    updateScrollBars();
    repaint();
}

void NoteEditor::zoomVertical (double factor, float anchorY)
{
    const double anchorRow = (anchorY - gridArea.getY() + scrollY) / rowHeight;
    rowHeight = (float) juce::jlimit (6.0, 32.0, rowHeight * factor);
    scrollY = anchorRow * rowHeight - (anchorY - gridArea.getY());
    updateScrollBars();
    repaint();
}

void NoteEditor::scrollToNotes()
{
    // Centre la vue sur les notes du clip (do 3 s'il est vide ; en haut de la drum map en mode batterie).
    if (drumMode)
        scrollY = 0.0;
    else
    {
        double centre = 60.0;
        if (! notes.empty())
        {
            int lo = 127, hi = 0;
            for (const auto& n : notes) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
            centre = (lo + hi) * 0.5;
        }
        scrollY = (127.0 - centre) * rowHeight - gridArea.getHeight() * 0.5;
    }
    scrollX = 0.0;
    updateScrollBars();
}

void NoteEditor::followPlayhead()
{
    // Défilement automatique : la page suit le curseur pendant la lecture.
    if (! location.playing || drag != Drag::none)
        return;
    const float x = xForBeat (location.position);
    if (x > gridArea.getRight() - 24 || x < gridArea.getX())
    {
        scrollX = juce::jmax (0.0, location.position * pixelsPerBeat - 24.0);
        updateScrollBars();
    }
}

//==============================================================================
void NoteEditor::resized()
{
    auto r = getLocalBounds();
    toolbarArea = r.removeFromTop (toolbarH);
    infoArea = r.removeFromTop (infoH);
    r.removeFromBottom (barT);
    const int kw = keysWidth();
    velocityArea = r.removeFromBottom (velocityH).withTrimmedLeft (kw).withTrimmedRight (barT);
    r.removeFromBottom (gap);
    rulerArea = r.removeFromTop (rulerH).withTrimmedLeft (kw).withTrimmedRight (barT);
    keysArea = r.withWidth (kw);
    gridArea = r.withTrimmedLeft (kw).withTrimmedRight (barT);
    hBar.setBounds (gridArea.getX(), getHeight() - barT, gridArea.getWidth(), barT);
    vBar.setBounds (gridArea.getRight(), gridArea.getY(), barT, gridArea.getHeight());

    // Barre d'outils : transport | outils | quantification | dessin | boucle | écoute, batterie | titre
    auto bar = toolbarArea.reduced (10, 10);
    playButton.setBounds (bar.removeFromLeft (32));
    bar.removeFromLeft (3);
    stopButton.setBounds (bar.removeFromLeft (32));
    bar.removeFromLeft (14);
    for (auto* b : tools)
    {
        b->setBounds (bar.removeFromLeft (30));
        bar.removeFromLeft (2);
    }
    bar.removeFromLeft (12);
    snapButton.setBounds (bar.removeFromLeft (62));
    bar.removeFromLeft (3);
    gridBox.setBounds (bar.removeFromLeft (74));
    bar.removeFromLeft (3);
    quantizeButton.setBounds (bar.removeFromLeft (28));
    bar.removeFromLeft (2);
    quantizeMore.setBounds (bar.removeFromLeft (28));
    lengthLabel.setBounds (bar.removeFromLeft (70));
    lengthBox.setBounds (bar.removeFromLeft (80));
    velocityLabel.setBounds (bar.removeFromLeft (40));
    velocityField->setBounds (bar.removeFromLeft (38));
    loopLabel.setBounds (bar.removeFromLeft (56));
    loopField->setBounds (bar.removeFromLeft (52));
    bar.removeFromLeft (2);
    loopMinus.setBounds (bar.removeFromLeft (22));
    bar.removeFromLeft (2);
    loopPlus.setBounds (bar.removeFromLeft (22));
    bar.removeFromLeft (12);
    listenButton.setBounds (bar.removeFromLeft (64));
    bar.removeFromLeft (3);
    drumButton.setBounds (bar.removeFromLeft (72));
    bar.removeFromLeft (10);
    title.setBounds (bar);

    // Ligne d'infos
    auto line = infoArea.reduced (10, 5);
    const int widths[] = { 92, 92, 92, 70, 48 };
    for (int i = 0; i < info.size(); ++i)
    {
        infoLabels[i]->setBounds (line.removeFromLeft (i == 0 ? 50 : 76));
        line.removeFromLeft (4);
        info[i]->setBounds (line.removeFromLeft (widths[i]));
    }

    if (! viewInitialised && gridArea.getWidth() > 0)
    {
        viewInitialised = true;
        pixelsPerBeat = juce::jlimit (16.0, 240.0, gridArea.getWidth() / juce::jmax (4.0, length + 0.5));
        rowHeight = drumMode ? 18.0f : 14.0f;
        scrollToNotes();
    }
    updateScrollBars();
}

void NoteEditor::updateToolbar()
{
    snapButton.setToggleState (snap, juce::dontSendNotification);
    listenButton.setToggleState (listen, juce::dontSendNotification);
    drumButton.setToggleState (drumMode, juce::dontSendNotification);
    playButton.setToggleState (location.playing || location.queued, juce::dontSendNotification);
    gridBox.setSelectedId (gridIndex + 1, juce::dontSendNotification);
    lengthBox.setSelectedId (lengthIndex + 1, juce::dontSendNotification);
    velocityField->show (juce::String (insertVelocity));
    const auto bars = notes::formatLength (length);   // « 2.0.0.0 » -> « 2.0 »
    loopField->show (bars.upToLastOccurrenceOf (".", false, false).upToLastOccurrenceOf (".", false, false));
}

int NoteEditor::firstSelected() const
{
    int first = selection.empty() ? -1 : *selection.begin();
    for (int i : selection)
        if (notes[(size_t) i].start < notes[(size_t) first].start)
            first = i;
    return first;
}

void NoteEditor::updateInfo()
{
    const bool any = ! selection.empty();
    for (auto* f : info)
        f->setEnabled (any);
    if (! any)
    {
        for (auto* f : info)
            f->show (juce::String::fromUTF8 ("—"));
        return;
    }
    // Comme Cubase : on montre la première note sélectionnée ; les changements s'appliquent à toutes.
    const auto& n = notes[(size_t) firstSelected()];
    info[0]->show (notes::formatPosition (n.start));
    info[1]->show (notes::formatPosition (n.start + n.length));
    info[2]->show (notes::formatLength (n.length));
    const auto drum = drumMode ? notes::drumName (n.pitch) : juce::String();
    info[3]->show (notes::pitchName (n.pitch) + (drum.isNotEmpty() ? "  " + drum : juce::String()));
    info[4]->show (juce::String (n.velocity));
}

//==============================================================================
// Suivi du clip (enregistrement, lecture, suppression)
NoteEditor::Location NoteEditor::locate() const
{
    return engine.read ([this] (const Session& s)
    {
        Location loc;
        for (int t = 0; t < s.song.numTracks(); ++t)
            for (int sc = 0; sc < s.song.numScenes(); ++sc)
                if (clip != nullptr && s.song.tracks[(size_t) t].slots[(size_t) sc] == clip)
                {
                    loc.track = t;
                    loc.scene = sc;
                    loc.drumMap = s.song.tracks[(size_t) t].drumMap;
                    loc.playing = s.getPlayingSlot (t) == sc && s.isPlaying();
                    loc.queued = s.getSlotState (t, sc) == SlotState::playQueued;
                    loc.position = s.getClipProgress (t) * clip->length;
                }
        return loc;
    });
}

void NoteEditor::timerCallback()
{
    location = locate();
    if (location.track < 0)
    {
        if (onClipGone)
            onClipGone();
        return;
    }

    if (location.drumMap != drumMode)
    {
        drumMode = location.drumMap;
        rebuildRows();
        resized();
        scrollToNotes();
        repaint();
    }

    // Les notes changent aussi ailleurs (enregistrement, collage dans la grille...).
    if (drag == Drag::none)
    {
        bool changed = false;
        engine.read ([&] (const Session&)
        {
            if (clip->notes.size() != notes.size() || std::abs (clip->length - length) > 1.0e-9)
                changed = true;
            else
                for (size_t i = 0; i < notes.size() && ! changed; ++i)
                {
                    const auto& a = clip->notes[i];
                    const auto& b = notes[i];
                    changed = std::abs (a.start - b.start) > 1.0e-9 || std::abs (a.length - b.length) > 1.0e-9
                           || a.pitch != b.pitch || a.velocity != b.velocity || a.muted != b.muted;
                }
            if (changed)
            {
                notes = clip->notes;
                length = clip->length;
            }
            return 0;
        });
        if (changed)
        {
            for (auto it = selection.begin(); it != selection.end();)
                it = *it >= (int) notes.size() ? selection.erase (it) : std::next (it);
            if (drumMode)
                rebuildRows();
            updateInfo();
            updateToolbar();
            updateScrollBars();
            repaint();
        }
    }

    const auto name = engine.read ([this] (const Session& s)
    {
        return s.song.tracks[(size_t) location.track].name + juce::String::fromUTF8 ("  ·  ") + (clip->name.isNotEmpty() ? clip->name : juce::String ("Clip"));
    });
    if (title.getText() != name)
        title.setText (name, juce::dontSendNotification);

    if (location.queued != playButton.getToggleState() && ! location.playing)
        updateToolbar();
    if (location.playing != wasPlaying)
    {
        // Arrêté depuis la grille : le curseur reste où la lecture s'est arrêtée.
        if (! location.playing && wasPlaying)
            cursorBeat = juce::jlimit (0.0, length, cursorBeatWhenPlaying);
        wasPlaying = location.playing;
        updateToolbar();
        repaint();
    }
    if (location.playing)
    {
        cursorBeatWhenPlaying = location.position;
        followPlayhead();
        repaint (gridArea.getUnion (rulerArea).getUnion (velocityArea));
    }
}

//==============================================================================
// Lecture depuis le curseur
void NoteEditor::playFromCursor()
{
    if (location.track < 0)
        return;
    const int t = location.track, s = location.scene;
    const double at = std::fmod (juce::jmax (0.0, cursorBeat), length);
    engine.perform ([&] (Session& session) { session.playClipFrom (t, s, at); });
    location = locate();
    updateToolbar();
}

void NoteEditor::stopPlayback()
{
    if (location.track < 0)
        return;
    if (location.playing || location.queued)
    {
        if (location.playing)
            cursorBeat = location.position;   // le curseur reste où on s'arrête
        const int t = location.track;
        engine.perform ([t] (Session& s) { s.stopTrackNow (t); });
        wasPlaying = false;
    }
    else
        cursorBeat = 0.0;                 // second stop : retour au début, comme Cubase
    location = locate();
    updateToolbar();
    repaint();
}

void NoteEditor::togglePlayback()
{
    if (location.playing || location.queued) stopPlayback();
    else playFromCursor();
}

void NoteEditor::moveCursor (double beat)
{
    cursorBeat = juce::jlimit (0.0, length, snap ? notes::snapNearest (beat, grid()) : beat);
    if (location.playing && location.track >= 0)
    {
        const int t = location.track;
        const double at = std::fmod (cursorBeat, length);
        engine.perform ([&] (Session& s) { s.setClipPosition (t, at); });
        cursorBeatWhenPlaying = at;
    }
    repaint();
}

//==============================================================================
// Dessin
void NoteEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    // Barre d'outils et ligne d'infos, dans l'esprit des façades des plugins.
    g.setGradientFill (juce::ColourGradient (theme::panel.brighter (0.06f), 0.0f, 0.0f, theme::panel, 0.0f, (float) toolbarH, false));
    g.fillRect (toolbarArea);
    g.setColour (theme::panel);
    g.fillRect (infoArea);
    g.setColour (theme::rule);
    g.fillRect (10, toolbarArea.getBottom() - 1, getWidth() - 20, 1);
    g.setColour (theme::line);
    g.fillRect (0, infoArea.getBottom() - 1, getWidth(), 1);

    paintGrid (g);
    paintNotes (g);
    paintKeys (g);
    paintRuler (g);
    paintVelocity (g);

    // Coins à gauche de la règle et de la piste des vélocités.
    g.setColour (theme::panel);
    g.fillRect (0, rulerArea.getY(), keysWidth(), rulerH);
    g.fillRect (0, velocityArea.getY(), keysWidth(), velocityH);
    theme::sectionTitle (g, drumMode ? "DRUM MAP" : "CLAVIER", { 8, rulerArea.getY(), keysWidth() - 12, rulerH });
    theme::sectionTitle (g, juce::String::fromUTF8 ("VÉLOCITÉ"), { 8, velocityArea.getY() + 4, keysWidth() - 12, 14 });
}

void NoteEditor::paintGrid (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (gridArea);
    g.setColour (juce::Colour (0xff242321));
    g.fillRect (gridArea);

    // Lignes : touches noires plus sombres (ou une ligne sur deux en drum map), filet à chaque do.
    const int first = rowAtY ((float) gridArea.getY()), last = rowAtY ((float) gridArea.getBottom());
    for (int r = first; r <= last; ++r)
    {
        const int p = pitchAtRow (r);
        const float y = (float) (gridArea.getY() + r * rowHeight - scrollY);
        const bool dark = drumMode ? (r % 2 == 1) : isBlackKey (p);
        if (dark)
        {
            g.setColour (juce::Colour (0xff1d1c1b));
            g.fillRect ((float) gridArea.getX(), y, (float) gridArea.getWidth(), rowHeight);
        }
        g.setColour (! drumMode && p % 12 == 0 ? juce::Colour (0xff3e3c39) : juce::Colour (0xff1a1a19));
        g.fillRect ((float) gridArea.getX(), y + rowHeight - 1.0f, (float) gridArea.getWidth(), 1.0f);
    }

    // Lignes verticales : mesures, temps, quantification.
    const double firstBeat = beatAtX ((float) gridArea.getX()), lastBeat = beatAtX ((float) gridArea.getRight());
    const double step = grid();
    if (step * pixelsPerBeat >= 5.0)
        for (double b = std::floor (firstBeat / step) * step; b <= lastBeat; b += step)
        {
            const bool isBeat = std::abs (b - std::round (b)) < 1.0e-6;
            g.setColour (isBeat ? juce::Colour (0xff363431) : juce::Colour (0xff2c2b29));
            g.fillRect (xForBeat (b), (float) gridArea.getY(), 1.0f, (float) gridArea.getHeight());
        }
    for (double b = std::floor (firstBeat / beatsPerBar) * beatsPerBar; b <= lastBeat; b += beatsPerBar)
    {
        g.setColour (juce::Colour (0xff57534c));
        g.fillRect (xForBeat (b), (float) gridArea.getY(), 1.5f, (float) gridArea.getHeight());
    }

    // Après la fin de la boucle : assombri.
    const float end = xForBeat (length);
    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillRect (end, (float) gridArea.getY(), (float) gridArea.getRight() - end, (float) gridArea.getHeight());
    g.setColour (theme::accent.withAlpha (0.85f));
    g.fillRect (end - 1.0f, (float) gridArea.getY(), 2.0f, (float) gridArea.getHeight());
}

void NoteEditor::paintNotes (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (gridArea);

    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const auto& n = notes[(size_t) i];
        const auto r = noteBounds (n);
        if (! r.intersects (gridArea.toFloat()))
            continue;
        const bool selected = selection.count (i) > 0;
        auto colour = notes::velocityColour (n.velocity);
        if (n.muted) colour = juce::Colour (0xff6a6762).withAlpha (0.45f);
        else if (n.start >= length) colour = colour.withMultipliedAlpha (0.4f);
        if (selected && ! n.muted) colour = colour.brighter (0.3f);

        if (drumMode)
        {
            juce::Path diamond;
            const auto c = r.getCentre();
            const float h = r.getHeight() * 0.5f - 0.5f;
            diamond.addQuadrilateral (c.x, c.y - h, c.x + h, c.y, c.x, c.y + h, c.x - h, c.y);
            g.setColour (colour);
            g.fillPath (diamond);
            g.setColour (selected ? juce::Colours::white : juce::Colours::black.withAlpha (0.6f));
            g.strokePath (diamond, juce::PathStrokeType (selected ? 1.6f : 1.0f));
            continue;
        }

        // Note façon plugin : léger dégradé, filet, nom si la place le permet.
        g.setGradientFill (juce::ColourGradient (colour.brighter (0.12f), 0.0f, r.getY(), colour.darker (0.1f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 2.5f);
        g.setColour (selected ? juce::Colours::white : juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (r, 2.5f, selected ? 1.6f : 1.0f);
        if (rowHeight >= 12.0f && r.getWidth() > 34.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.setFont (theme::font (juce::jmin (11.0f, rowHeight - 3.0f), juce::Font::bold));
            g.drawText (notes::pitchName (n.pitch), r.reduced (4, 0), juce::Justification::centredLeft, false);
        }
    }

    if (drag == Drag::rubber)
    {
        g.setColour (theme::selection.withAlpha (0.1f));
        g.fillRect (rubber);
        g.setColour (theme::selection.withAlpha (0.7f));
        g.drawRect (rubber, 1.0f);
    }

    // Curseur de lecture (comme Cubase : une ligne, qui avance pendant la lecture).
    const double at = location.playing ? location.position : cursorBeat;
    g.setColour (location.playing ? theme::play : theme::text.withAlpha (0.85f));
    g.fillRect (xForBeat (at) - 0.5f, (float) gridArea.getY(), 1.5f, (float) gridArea.getHeight());
}

void NoteEditor::paintKeys (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (keysArea);
    const int first = rowAtY ((float) keysArea.getY()), last = rowAtY ((float) keysArea.getBottom());

    if (drumMode)
    {
        // Drum map : le nom de chaque son et sa note.
        g.setColour (theme::panel);
        g.fillRect (keysArea);
        for (int r = first; r <= last; ++r)
        {
            const int p = pitchAtRow (r);
            const auto row = juce::Rectangle<float> ((float) keysArea.getX(), (float) (gridArea.getY() + r * rowHeight - scrollY),
                                                     (float) keysArea.getWidth(), rowHeight);
            g.setColour (p == previewPitch ? theme::accent : r % 2 == 1 ? juce::Colour (0xff171717) : theme::panel);
            g.fillRect (row);
            const auto name = notes::drumName (p);
            g.setColour (p == previewPitch ? juce::Colours::black : theme::text);
            g.setFont (theme::font (juce::jmin (12.0f, rowHeight - 3.0f)));
            g.drawText (name.isNotEmpty() ? name : juce::String ("Note"), row.withTrimmedLeft (8).withTrimmedRight (34), juce::Justification::centredLeft, true);
            g.setColour (p == previewPitch ? juce::Colours::black.withAlpha (0.6f) : theme::textDim);
            g.setFont (theme::font (10.0f));
            g.drawText (notes::pitchName (p), row.withTrimmedRight (6), juce::Justification::centredRight);
        }
    }
    else
    {
        g.setColour (juce::Colour (0xffece9e0));
        g.fillRect (keysArea);
        for (int r = first; r <= last; ++r)
        {
            const int p = pitchAtRow (r);
            const float y = (float) (gridArea.getY() + r * rowHeight - scrollY);
            if (isBlackKey (p))
            {
                g.setColour (p == previewPitch ? theme::accent : juce::Colour (0xff1a1918));
                g.fillRoundedRectangle ((float) keysArea.getX() - 3.0f, y + 0.5f, keysWidth() * 0.62f + 3.0f, rowHeight - 1.0f, 2.0f);
            }
            else
            {
                if (p == previewPitch)
                {
                    g.setColour (theme::accent);
                    g.fillRect ((float) keysArea.getX(), y, (float) keysWidth(), rowHeight);
                }
                if (p % 12 == 0 || p % 12 == 5)
                {
                    g.setColour (juce::Colour (0xff9c988f));
                    g.fillRect ((float) keysArea.getX(), y + rowHeight - 0.5f, (float) keysWidth(), 1.0f);
                }
            }
            if (p % 12 == 0 && rowHeight >= 8.0f)
            {
                g.setColour (juce::Colour (0xff4a4640));
                g.setFont (theme::font (juce::jmin (11.0f, rowHeight), juce::Font::bold));
                g.drawText (notes::pitchName (p), keysArea.getX(), (int) y, keysWidth() - 5, (int) rowHeight, juce::Justification::centredRight);
            }
        }
    }
    g.setColour (theme::line);
    g.fillRect (keysArea.getRight() - 1, keysArea.getY(), 1, keysArea.getHeight());
}

void NoteEditor::paintRuler (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (rulerArea);
    g.setGradientFill (juce::ColourGradient (theme::panelLight, 0.0f, (float) rulerArea.getY(), theme::panel, 0.0f, (float) rulerArea.getBottom(), false));
    g.fillRect (rulerArea);
    const double firstBeat = beatAtX ((float) rulerArea.getX()), lastBeat = beatAtX ((float) rulerArea.getRight());

    // Zone de la boucle
    g.setColour (theme::accent.withAlpha (0.22f));
    g.fillRect (juce::Rectangle<float> (xForBeat (0.0), (float) rulerArea.getY(), (float) (length * pixelsPerBeat), (float) rulerH)
                    .getIntersection (rulerArea.toFloat()));

    g.setFont (theme::font (11.0f, juce::Font::bold));
    for (double b = std::floor (firstBeat); b <= lastBeat; b += 1.0)
    {
        const float x = xForBeat (b);
        const bool isBar = (int) std::round (b) % beatsPerBar == 0;
        g.setColour (isBar ? theme::text : theme::textDim);
        g.fillRect (x, isBar ? (float) rulerArea.getY() + 4.0f : (float) rulerArea.getBottom() - 7.0f, 1.0f, isBar ? (float) rulerH - 4.0f : 7.0f);
        if (isBar)
            g.drawText (juce::String ((int) std::round (b) / beatsPerBar + 1), (int) x + 4, rulerArea.getY(), 40, rulerH - 6, juce::Justification::centredLeft);
    }

    // Fin de boucle, à faire glisser.
    const float end = xForBeat (length);
    juce::Path handle;
    handle.addTriangle (end - 8.0f, (float) rulerArea.getY() + 2.0f, end + 1.0f, (float) rulerArea.getY() + 2.0f, end + 1.0f, (float) rulerArea.getBottom() - 2.0f);
    g.setColour (theme::accent);
    g.fillPath (handle);

    // Curseur : tête triangulaire, à faire glisser dans la règle (même pendant la lecture).
    const double at = location.playing ? location.position : cursorBeat;
    const float x = xForBeat (at);
    juce::Path head;
    head.addTriangle (x - 6.0f, (float) rulerArea.getBottom() - 11.0f, x + 6.0f, (float) rulerArea.getBottom() - 11.0f, x, (float) rulerArea.getBottom());
    g.setColour (location.playing ? theme::play : theme::text);
    g.fillPath (head);
    g.fillRect (x - 0.5f, (float) rulerArea.getY(), 1.5f, (float) rulerH);
}

void NoteEditor::paintVelocity (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (velocityArea);
    g.setColour (juce::Colour (0xff1c1b1a));
    g.fillRect (velocityArea);
    g.setColour (juce::Colour (0xff2a2826));
    for (int v : { 32, 64, 96 })
    {
        const float y = (float) velocityArea.getBottom() - velocityArea.getHeight() * v / 127.0f;
        g.fillRect ((float) velocityArea.getX(), y, (float) velocityArea.getWidth(), 1.0f);
    }
    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const auto& n = notes[(size_t) i];
        const float x = xForBeat (n.start);
        if (x < velocityArea.getX() - 4 || x > velocityArea.getRight() + 4)
            continue;
        const float h = (velocityArea.getHeight() - 6.0f) * n.velocity / 127.0f;
        const bool selected = selection.count (i) > 0;
        g.setColour (n.muted ? juce::Colour (0xff6a6762) : notes::velocityColour (n.velocity).withMultipliedBrightness (selected ? 1.15f : 0.8f));
        g.fillRect (x, (float) velocityArea.getBottom() - h, 3.0f, h);
        g.fillEllipse (x - 2.0f, (float) velocityArea.getBottom() - h - 3.0f, 7.0f, 7.0f);
        if (selected)
        {
            g.setColour (juce::Colours::white);
            g.drawEllipse (x - 2.0f, (float) velocityArea.getBottom() - h - 3.0f, 7.0f, 7.0f, 1.2f);
        }
    }
    const double at = location.playing ? location.position : cursorBeat;
    g.setColour ((location.playing ? theme::play : theme::text).withAlpha (0.5f));
    g.fillRect (xForBeat (at), (float) velocityArea.getY(), 1.0f, (float) velocityArea.getHeight());
}

//==============================================================================
// Écoute
void NoteEditor::preview (int pitch, int velocity)
{
    if (! listen || location.track < 0 || previewPitch == pitch)
        return;
    stopPreview();
    previewPitch = pitch;
    engine.previewNote (location.track, pitch, velocity);
    repaint (keysArea);
}

void NoteEditor::stopPreview()
{
    if (previewPitch >= 0 && location.track >= 0)
        engine.previewNote (location.track, previewPitch, 0);
    previewPitch = -1;
    repaint (keysArea);
}

void NoteEditor::previewBriefly (int pitch, int velocity)
{
    preview (pitch, velocity);
    juce::Timer::callAfterDelay (180, [safe = juce::Component::SafePointer<NoteEditor> (this)] { if (safe) safe->stopPreview(); });
}

//==============================================================================
// Boîte à outils
void NoteEditor::openToolbox (juce::Point<int> at)
{
    closeToolbox();
    toolbox = std::make_unique<Toolbox> (*this);
    auto bounds = toolbox->getBounds().withPosition (at.translated (-18, -18));
    bounds = bounds.constrainedWithin (getLocalBounds().reduced (4));
    toolbox->setBounds (bounds);
    addAndMakeVisible (*toolbox);
    toolboxOpenedAt = juce::Time::getMillisecondCounter();
}

void NoteEditor::closeToolbox()
{
    toolbox.reset();
}

void NoteEditor::runCommand (int id)
{
    closeToolbox();
    if (id >= toolBase)
    {
        setTool ((Tool) (id - toolBase));
        return;
    }
    switch (id)
    {
        case quantizeCmd:        quantize(); break;
        case quantizeLengthsCmd: commit (true, [this] { notes::quantizeLengths (notes, selection, grid()); }); break;
        case legatoCmd:          commit (true, [this] { notes::legato (notes, selection, length); }); break;
        case muteCmd:
        case unmuteCmd:          commit (true, [this, mute = id == muteCmd] { for (int i : selection) notes[(size_t) i].muted = mute; }); break;
        case duplicateCmd:       duplicateSelection(); break;
        case deleteCmd:          deleteSelection(); break;
        case selectAllCmd:       selectAll(); break;
        default: break;
    }
}

void NoteEditor::showQuantizePanel()
{
    juce::CallOutBox::launchAsynchronously (std::make_unique<QuantizePanel> (*this), quantizeMore.getScreenBounds(), nullptr);
}

//==============================================================================
// Souris
void NoteEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (rulerArea.contains (p.toInt()))
    {
        setMouseCursor (std::abs (p.x - xForBeat (length)) < 8.0f ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
        return;
    }
    if (tool != Tool::select || ! gridArea.contains (p.toInt()))
    {
        setTool (tool);
        return;
    }
    const int i = noteAt (p);
    if (i >= 0 && ! drumMode)
    {
        const auto r = noteBounds (notes[(size_t) i]);
        const float edge = juce::jmin (6.0f, r.getWidth() / 3.0f);
        if (p.x < r.getX() + edge || p.x > r.getRight() - edge)
        {
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
    }
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void NoteEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto p = e.position;
    dragStart = p;
    dragChanged = false;
    copyOnDrag = false;
    original = notes;
    drag = Drag::none;

    // Un clic hors de la boîte à outils la referme.
    if (toolbox != nullptr)
    {
        closeToolbox();
        if (! e.mods.isPopupMenu())
            return;
    }

    // Clic droit : boîte à outils, comme dans Cubase.
    if (e.mods.isPopupMenu())
    {
        if (gridArea.contains (p.toInt()))
        {
            const int i = noteAt (p);
            if (i >= 0 && selection.count (i) == 0)
                selection = { i };
            updateInfo();
            repaint();
        }
        openToolbox (p.toInt());
        drag = Drag::toolbox;
        return;
    }

    // Clavier ou drum map : écoute.
    if (keysArea.contains (p.toInt()))
    {
        drag = Drag::keyboard;
        const bool was = listen;
        listen = true;
        preview (pitchAtY (p.y), insertVelocity);
        listen = was;
        return;
    }

    // Règle : fin de boucle, ou curseur de lecture (déplacé aussi pendant la lecture).
    if (rulerArea.contains (p.toInt()))
    {
        if (std::abs (p.x - xForBeat (length)) < 8.0f)
        {
            pushUndo();
            drag = Drag::loopEnd;
        }
        else
        {
            drag = Drag::cursor;
            rulerZooming = false;
            zoomStartPixels = pixelsPerBeat;
            zoomAnchorBeat = beatAtX (p.x);
            moveCursor (beatAtX (p.x));
        }
        return;
    }

    // Piste des vélocités : on dessine les vélocités des notes sous la souris.
    if (velocityArea.contains (p.toInt()))
    {
        pushUndo();
        drag = Drag::velocityLane;
        mouseDrag (e);
        return;
    }

    if (! gridArea.contains (p.toInt()))
        return;

    const int hit = noteAt (p);
    const double beat = beatAtX (p.x);
    const int pitch = pitchAtY (p.y);

    switch (tool)
    {
        case Tool::select:
        {
            if (hit < 0)
            {
                if (! e.mods.isShiftDown() && ! e.mods.isCommandDown())
                    selection.clear();
                baseSelection = selection;
                rubber = juce::Rectangle<float> (p, p);
                drag = Drag::rubber;
                updateInfo();
                repaint();
                return;
            }
            if (e.mods.isCommandDown() && ! e.mods.isShiftDown())
            {
                if (selection.count (hit) > 0) selection.erase (hit);
                else selection.insert (hit);
                updateInfo();
                repaint();
                return;
            }
            if (selection.count (hit) == 0)
            {
                if (! e.mods.isShiftDown())
                    selection.clear();
                selection.insert (hit);
            }
            dragIndex = hit;
            const auto r = noteBounds (notes[(size_t) hit]);
            const float edge = juce::jmin (6.0f, r.getWidth() / 3.0f);
            if (e.mods.isCommandDown() && e.mods.isShiftDown())
                drag = Drag::velocity;
            else if (! drumMode && p.x < r.getX() + edge)
                drag = Drag::resizeStart;
            else if (! drumMode && p.x > r.getRight() - edge)
                drag = Drag::resizeEnd;
            else
            {
                drag = Drag::move;
                copyOnDrag = e.mods.isAltDown();
            }
            preview (notes[(size_t) hit].pitch, notes[(size_t) hit].velocity);
            updateInfo();
            repaint();
            return;
        }

        case Tool::draw:
        {
            if (hit >= 0)
            {
                // Le crayon sur une note l'efface (comme Cubase et sa baguette de batterie).
                commit (true, [&] { notes::erase (notes, { hit }); selection.clear(); });
                return;
            }
            pushUndo();
            const double start = juce::jmax (0.0, snap ? notes::snapFloor (beat, grid()) : beat);
            notes.push_back ({ start, insertLength(), pitch, insertVelocity });
            selection = { (int) notes.size() - 1 };
            dragIndex = (int) notes.size() - 1;
            original = notes;
            drag = drumMode ? Drag::none : Drag::draw;
            sendToClip();
            preview (pitch, insertVelocity);
            updateInfo();
            repaint();
            return;
        }

        case Tool::erase:
            pushUndo();
            drag = Drag::erase;
            if (hit >= 0)
            {
                notes::erase (notes, { hit });
                selection.clear();
                sendToClip();
                repaint();
            }
            return;

        case Tool::mute:
            if (hit >= 0)
                toggleMute (hit);
            return;

        case Tool::split:
            if (hit >= 0 && ! drumMode)
            {
                const double at = snap ? notes::snapNearest (beat, grid()) : beat;
                commit (true, [&] { selection = notes::splitAt (notes, selection, hit, at); });
            }
            return;

        case Tool::glue:
            if (hit >= 0)
                commit (true, [&] { const int kept = notes::glue (notes, hit); selection = { kept }; });
            return;

        case Tool::zoom:
            zoomHorizontal (e.mods.isAltDown() ? 0.5 : 2.0, p.x);
            return;
    }
}

void NoteEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const float dxPixels = p.x - dragStart.x, dyPixels = p.y - dragStart.y;

    switch (drag)
    {
        case Drag::toolbox:
            if (toolbox != nullptr)
                toolbox->setHover (toolbox->itemAt (toolbox->getLocalPoint (this, p.toInt())));
            return;

        case Drag::keyboard:
        {
            const bool was = listen;
            listen = true;
            preview (pitchAtY (p.y), insertVelocity);
            listen = was;
            return;
        }

        case Drag::cursor:
        {
            // Comme la règle de Cubase : glisser vers le bas zoome, vers le haut dézoome,
            // et la mesure cliquée reste sous la souris (on peut aussi glisser de côté).
            if (! rulerZooming && std::abs (dyPixels) > 5.0f && std::abs (dyPixels) > std::abs (dxPixels))
                rulerZooming = true;
            if (rulerZooming)
            {
                pixelsPerBeat = juce::jlimit (12.0, 600.0, zoomStartPixels * std::exp (dyPixels * 0.012));
                scrollX = zoomAnchorBeat * pixelsPerBeat - (p.x - gridArea.getX());
                updateScrollBars();
                repaint();
            }
            else
                moveCursor (beatAtX (p.x));
            return;
        }

        case Drag::loopEnd:
        {
            const double step = snap ? juce::jmax (1.0, grid()) : grid();
            length = juce::jmax (1.0, notes::snapNearest (beatAtX (p.x), step));
            sendToClip();
            updateToolbar();
            repaint();
            return;
        }

        case Drag::velocityLane:
        {
            const int v = juce::jlimit (1, 127, (int) std::round ((velocityArea.getBottom() - p.y) / (velocityArea.getHeight() - 6.0f) * 127.0f));
            bool any = false;
            for (int i = 0; i < (int) notes.size(); ++i)
                if (std::abs (xForBeat (notes[(size_t) i].start) - p.x) <= 4.0f && (selection.empty() || selection.count (i) > 0))
                {
                    notes[(size_t) i].velocity = v;
                    any = true;
                }
            if (any)
            {
                sendToClip();
                updateInfo();
                repaint();
            }
            return;
        }

        case Drag::rubber:
        {
            rubber = juce::Rectangle<float> (dragStart, p);
            selection = baseSelection;
            for (int i = 0; i < (int) notes.size(); ++i)
                if (noteBounds (notes[(size_t) i]).intersects (rubber))
                    selection.insert (i);
            updateInfo();
            repaint();
            return;
        }

        case Drag::erase:
        {
            const int hit = noteAt (p);
            if (hit >= 0)
            {
                notes::erase (notes, { hit });
                selection.clear();
                sendToClip();
                repaint();
            }
            return;
        }

        case Drag::draw:
        {
            auto& n = notes[(size_t) dragIndex];
            const double end = beatAtX (p.x);
            const double len = snap ? juce::jmax (grid(), notes::snapNearest (end - n.start, grid())) : juce::jmax (1.0 / 64.0, end - n.start);
            if (std::abs (len - n.length) > 1.0e-9)
            {
                n.length = juce::jmax (insertLength(), len);
                sendToClip();
                updateInfo();
                repaint();
            }
            return;
        }

        case Drag::move:
        case Drag::resizeStart:
        case Drag::resizeEnd:
        case Drag::velocity:
            break;

        case Drag::none:
            return;
    }

    if (! dragChanged && std::abs (dxPixels) < 3.0f && std::abs (dyPixels) < 3.0f)
        return;
    if (! dragChanged)
    {
        pushUndo();
        dragChanged = true;
        if (copyOnDrag)
        {
            // Alt-glisser : on déplace des copies, les originaux restent.
            notes::Selection copies;
            for (int i : selection)
            {
                notes.push_back (notes[(size_t) i]);
                copies.insert ((int) notes.size() - 1);
                if (i == dragIndex)
                    dragIndex = (int) notes.size() - 1;
            }
            selection = copies;
            original = notes;
        }
    }

    const auto& anchor = original[(size_t) dragIndex];
    const double minimum = snap ? grid() : 1.0 / 64.0;

    if (drag == Drag::move)
    {
        bool horizontal = true, vertical = true;
        if (e.mods.isCommandDown())   // Cmd : un seul sens à la fois
            (std::abs (dxPixels) > std::abs (dyPixels) ? vertical : horizontal) = false;

        double target = anchor.start + (horizontal ? dxPixels / pixelsPerBeat : 0.0);
        if (snap && horizontal)
            target = notes::snapNearest (target, grid());
        double delta = target - anchor.start;
        double earliest = 1.0e9;
        int topRow = numRows(), bottomRow = 0;
        for (int i : selection)
        {
            earliest = juce::jmin (earliest, original[(size_t) i].start);
            topRow = juce::jmin (topRow, rowOf (original[(size_t) i].pitch));
            bottomRow = juce::jmax (bottomRow, rowOf (original[(size_t) i].pitch));
        }
        delta = juce::jmax (-earliest, delta);
        int rowDelta = vertical ? (int) std::round (dyPixels / rowHeight) : 0;
        rowDelta = juce::jlimit (-topRow, numRows() - 1 - bottomRow, rowDelta);
        for (int i : selection)
        {
            notes[(size_t) i].start = original[(size_t) i].start + delta;
            notes[(size_t) i].pitch = pitchAtRow (rowOf (original[(size_t) i].pitch) + rowDelta);
        }
        preview (notes[(size_t) dragIndex].pitch, notes[(size_t) dragIndex].velocity);
    }
    else if (drag == Drag::resizeEnd)
    {
        double end = anchor.start + anchor.length + dxPixels / pixelsPerBeat;
        if (snap)
            end = notes::snapNearest (end, grid());
        const double delta = end - (anchor.start + anchor.length);
        for (int i : selection)
            notes[(size_t) i].length = juce::jmax (minimum, original[(size_t) i].length + delta);
    }
    else if (drag == Drag::resizeStart)
    {
        double start = anchor.start + dxPixels / pixelsPerBeat;
        if (snap)
            start = notes::snapNearest (start, grid());
        const double delta = start - anchor.start;
        for (int i : selection)
        {
            const auto& o = original[(size_t) i];
            const double end = o.start + o.length;
            const double s = juce::jlimit (0.0, end - minimum, o.start + delta);
            notes[(size_t) i].start = s;
            notes[(size_t) i].length = end - s;
        }
    }
    else if (drag == Drag::velocity)
    {
        const int delta = (int) std::round (-dyPixels / 2.0f);
        for (int i : selection)
            notes[(size_t) i].velocity = juce::jlimit (1, 127, original[(size_t) i].velocity + delta);
    }

    sendToClip();
    updateInfo();
    updateScrollBars();
    repaint();
}

void NoteEditor::mouseUp (const juce::MouseEvent& e)
{
    if (drag == Drag::toolbox)
    {
        // Bouton droit maintenu puis relâché sur un choix : on l'applique (comme Cubase).
        // Simple clic : la boîte reste ouverte et se manie à la souris.
        drag = Drag::none;
        if (toolbox != nullptr)
        {
            const int id = toolbox->itemAt (toolbox->getLocalPoint (this, e.getPosition()));
            const bool held = e.getDistanceFromDragStart() > 4 || juce::Time::getMillisecondCounter() - toolboxOpenedAt > 350;
            if (held && id != 0)
                runCommand (id);
            else if (held && id == 0)
                closeToolbox();
        }
        return;
    }

    if (drag == Drag::move && ! dragChanged && ! e.mods.isShiftDown() && dragIndex >= 0)
        selection = { dragIndex };   // simple clic sur une note déjà sélectionnée : elle seule
    if ((drag == Drag::velocityLane || drag == Drag::erase || drag == Drag::loopEnd) && ! undoStack.empty()
        && undoStack.back().first.size() == notes.size() && std::abs (undoStack.back().second - length) < 1.0e-9)
    {
        bool same = true;
        for (size_t i = 0; i < notes.size() && same; ++i)
            same = undoStack.back().first[i].velocity == notes[i].velocity;
        if (same)
            undoStack.pop_back();   // rien n'a changé : on n'encombre pas l'annulation
    }
    if (drumMode && (drag == Drag::move || drag == Drag::draw))
        rebuildRows();
    drag = Drag::none;
    stopPreview();
    updateInfo();
    updateToolbar();
    repaint();
}

void NoteEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Double-clic sur une note avec la sélection : toutes les notes de cette hauteur.
    if (tool == Tool::select && gridArea.contains (e.position.toInt()) && ! e.mods.isPopupMenu())
    {
        const int hit = noteAt (e.position);
        if (hit >= 0)
        {
            selection.clear();
            for (int i = 0; i < (int) notes.size(); ++i)
                if (notes[(size_t) i].pitch == notes[(size_t) hit].pitch)
                    selection.insert (i);
            updateInfo();
            repaint();
        }
    }
}

void NoteEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        zoomHorizontal (w.deltaY > 0 ? 1.15 : 1.0 / 1.15, e.position.x);
        return;
    }
    if (e.mods.isAltDown())
    {
        zoomVertical (w.deltaY > 0 ? 1.1 : 1.0 / 1.1, e.position.y);
        return;
    }
    const bool horizontal = e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY);
    if (horizontal)
        scrollX -= (std::abs (w.deltaX) > 0.0f ? w.deltaX : w.deltaY) * 300.0;
    else
        scrollY -= w.deltaY * 300.0;
    updateScrollBars();
    repaint();
}

//==============================================================================
// Actions
void NoteEditor::quantize()
{
    auto q = quantizeSettings;
    q.grid = grid();
    commit (true, [&] { notes::quantize (notes, selection, q); });
}

void NoteEditor::selectAll()
{
    selection = notes::allOrSelected (notes, {});
    updateInfo();
    repaint();
}

void NoteEditor::deleteSelection()
{
    if (selection.empty())
        return;
    commit (true, [&] { notes::erase (notes, selection); selection.clear(); });
}

void NoteEditor::copySelection (bool cut)
{
    if (selection.empty())
        return;
    clipboard.clear();
    double first = 1.0e9;
    for (int i : selection)
        first = juce::jmin (first, notes[(size_t) i].start);
    for (int i : selection)
    {
        auto n = notes[(size_t) i];
        n.start -= first;
        clipboard.push_back (n);
    }
    if (cut)
        deleteSelection();
}

void NoteEditor::paste()
{
    // Colle au curseur, comme Cubase.
    if (clipboard.empty())
        return;
    commit (true, [&]
    {
        selection.clear();
        for (auto n : clipboard)
        {
            n.start += cursorBeat;
            notes.push_back (n);
            selection.insert ((int) notes.size() - 1);
        }
    });
}

void NoteEditor::duplicateSelection()
{
    if (! selection.empty())
        commit (true, [&] { selection = notes::duplicate (notes, selection); });
}

void NoteEditor::moveRows (int rowDelta)
{
    // Flèches haut / bas : un demi-ton au clavier, un son de la drum map en mode batterie.
    if (selection.empty() || rowDelta == 0)
        return;
    int top = numRows(), bottom = 0;
    for (int i : selection)
    {
        top = juce::jmin (top, rowOf (notes[(size_t) i].pitch));
        bottom = juce::jmax (bottom, rowOf (notes[(size_t) i].pitch));
    }
    if (top + rowDelta < 0 || bottom + rowDelta >= numRows())
        return;
    commit (true, [&] { for (int i : selection) notes[(size_t) i].pitch = pitchAtRow (rowOf (notes[(size_t) i].pitch) + rowDelta); });
    const int first = firstSelected();
    previewBriefly (notes[(size_t) first].pitch, notes[(size_t) first].velocity);
}

void NoteEditor::nudgeSelection (double beats)
{
    if (! selection.empty())
        commit (true, [&] { notes::nudge (notes, selection, beats); });
}

void NoteEditor::resizeSelection (double beats)
{
    if (! selection.empty())
        commit (true, [&] { notes::resize (notes, selection, beats, grid()); });
}

void NoteEditor::selectNeighbour (int direction, bool extend)
{
    if (notes.empty())
        return;
    std::vector<int> order ((size_t) notes.size());
    for (int i = 0; i < (int) order.size(); ++i) order[(size_t) i] = i;
    std::sort (order.begin(), order.end(), [this] (int a, int b)
               { return notes[(size_t) a].start < notes[(size_t) b].start
                     || (std::abs (notes[(size_t) a].start - notes[(size_t) b].start) < 1.0e-9 && notes[(size_t) a].pitch < notes[(size_t) b].pitch); });
    int current = -1;
    for (int i : selection)
    {
        const int pos = (int) (std::find (order.begin(), order.end(), i) - order.begin());
        current = current < 0 ? pos : direction > 0 ? juce::jmax (current, pos) : juce::jmin (current, pos);
    }
    const int next = juce::jlimit (0, (int) order.size() - 1, current < 0 ? 0 : current + direction);
    if (! extend)
        selection.clear();
    selection.insert (order[(size_t) next]);
    const auto& n = notes[(size_t) order[(size_t) next]];
    previewBriefly (n.pitch, n.velocity);
    updateInfo();
    repaint();
}

void NoteEditor::toggleMute (int index)
{
    commit (true, [&]
    {
        const auto targets = selection.count (index) > 0 ? selection : notes::Selection { index };
        const bool mute = ! notes[(size_t) index].muted;
        for (int i : targets)
            notes[(size_t) i].muted = mute;
    });
}

void NoteEditor::setLoopLength (double beats)
{
    const double newLength = juce::jlimit (1.0, 1024.0, beats);
    if (std::abs (newLength - length) > 1.0e-9)
        commit (true, [&] { length = newLength; });
    updateToolbar();
    updateScrollBars();
}

//==============================================================================
// Raccourcis (ceux de Cubase)
bool NoteEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const bool cmd = mods.isCommandDown() || mods.isCtrlDown();
    const int code = key.getKeyCode();
    const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    if (toolbox != nullptr && code == juce::KeyPress::escapeKey)
    {
        closeToolbox();
        return true;
    }

    if (cmd)
    {
        if (code == 'A' || c == 'a') { selectAll(); return true; }
        if (code == 'C' || c == 'c') { copySelection (false); return true; }
        if (code == 'X' || c == 'x') { copySelection (true); return true; }
        if (code == 'V' || c == 'v') { paste(); return true; }
        if (code == 'D' || c == 'd') { duplicateSelection(); return true; }
        if (code == 'Z' || c == 'z') { if (mods.isShiftDown()) redo(); else undo(); return true; }
        if (code == 'W' || c == 'w') { if (onCloseRequested) onCloseRequested(); return true; }
        if (code == juce::KeyPress::leftKey)  { nudgeSelection (-grid()); return true; }
        if (code == juce::KeyPress::rightKey) { nudgeSelection (grid()); return true; }
        return false;
    }

    if (mods.isAltDown() && (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey))
    {
        resizeSelection (code == juce::KeyPress::leftKey ? -grid() : grid());
        return true;
    }

    if (code == juce::KeyPress::upKey)    { moveRows (mods.isShiftDown() && ! drumMode ? -12 : -1); return true; }
    if (code == juce::KeyPress::downKey)  { moveRows (mods.isShiftDown() && ! drumMode ? 12 : 1); return true; }
    if (code == juce::KeyPress::leftKey)  { selectNeighbour (-1, mods.isShiftDown()); return true; }
    if (code == juce::KeyPress::rightKey) { selectNeighbour (1, mods.isShiftDown()); return true; }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { deleteSelection(); return true; }
    if (code == juce::KeyPress::spaceKey) { togglePlayback(); return true; }
    if (code == juce::KeyPress::escapeKey)
    {
        if (! selection.empty()) { selection.clear(); updateInfo(); repaint(); }
        else if (onCloseRequested) onCloseRequested();
        return true;
    }
    if (code == juce::KeyPress::returnKey)
    {
        // Entrée : lance ce clip dans la grille, calé sur la mesure.
        if (location.track >= 0)
            engine.perform ([this] (Session& s) { s.launchSlot (location.track, location.scene); });
        return true;
    }
    if (code == juce::KeyPress::homeKey) { moveCursor (0.0); return true; }

    switch (c)
    {
        case '1': setTool (Tool::select); return true;
        case '3': setTool (Tool::split); return true;
        case '4': setTool (Tool::glue); return true;
        case '5': setTool (Tool::erase); return true;
        case '6': setTool (Tool::zoom); return true;
        case '7': setTool (Tool::mute); return true;
        case '8': setTool (Tool::draw); return true;
        case 'j': snap = ! snap; updateToolbar(); return true;
        case 'q': quantize(); return true;
        case 'g': if (mods.isShiftDown()) zoomVertical (1.0 / 1.25, (float) gridArea.getCentreY()); else zoomHorizontal (1.0 / 1.4, (float) gridArea.getCentreX()); return true;
        case 'h': if (mods.isShiftDown()) zoomVertical (1.25, (float) gridArea.getCentreY()); else zoomHorizontal (1.4, (float) gridArea.getCentreX()); return true;
        default: break;
    }
    return false;
}
} // namespace gridplay
