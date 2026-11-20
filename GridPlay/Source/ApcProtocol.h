// Protocole de l'Akai APC mini (mk1 et mk2) : quelles notes envoient les boutons,
// et quels messages allument les LED. Sans état, pour pouvoir le tester.
//
// Grille 8 x 8 : note 0 = en bas à gauche, 63 = en haut à droite (les deux modèles).
// mk1 : boutons du bas 64-71, boutons de droite (scènes) 82-89, SHIFT 98.
//       LED de la grille par la vélocité : 0 éteint, 1 vert, 2 vert clignotant,
//       3 rouge, 4 rouge clignotant, 5 jaune, 6 jaune clignotant.
// mk2 : boutons du bas 100-107, scènes 112-119, SHIFT 122.
//       LED RVB : la vélocité choisit la couleur dans la palette, le canal la luminosité
//       (canal 1 à 7 = 10 % à 100 %).
// Les deux : boutons simples (bas, scènes) 0 éteint, 1 allumé, 2 clignotant ;
//            9 faders en CC 48 à 56 (le dernier = master).
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>

namespace gridplay::apc
{
enum class Model { none, mk1, mk2 };

inline Model modelFromName (const juce::String& name)
{
    const auto n = name.toLowerCase();
    if (! n.contains ("apc mini") && ! n.contains ("apc_mini") && ! n.contains ("apcmini"))
        return Model::none;
    return n.contains ("mk2") || n.contains ("mkii") ? Model::mk2 : Model::mk1;
}

// Le mk2 a deux ports : « Control » (pads et LED) et « Notes » (mode clavier).
inline int portScore (const juce::String& name)
{
    if (modelFromName (name) == Model::none)
        return -1;
    const auto n = name.toLowerCase();
    int score = 1;
    if (n.contains ("control")) score += 2;
    if (n.contains ("notes") || n.startsWith ("midiin2") || n.startsWith ("midiout2")) score -= 2;
    return score;
}

constexpr int gridSize = 8;
constexpr int firstFaderCC = 48;
constexpr int numFaders = 9;

inline int gridNote (int row, int column)            { return (gridSize - 1 - row) * gridSize + column; }
inline int trackButtonNote (Model m, int i)          { return (m == Model::mk2 ? 100 : 64) + i; }
inline int sceneButtonNote (Model m, int i)          { return (m == Model::mk2 ? 112 : 82) + i; }
inline int shiftNote (Model m)                       { return m == Model::mk2 ? 122 : 98; }

//==============================================================================
struct Input
{
    enum class Kind { none, pad, trackButton, sceneButton, shift, fader };
    Kind kind = Kind::none;
    int row = 0, column = 0;   // pad : ligne 0 = en haut
    int index = 0;             // bouton ou fader
    bool pressed = false;
    float value = 0.0f;        // fader : 0..1
};

inline Input parse (Model model, const juce::MidiMessage& m)
{
    Input in;
    if (model == Model::none)
        return in;

    if (m.isController())
    {
        const int i = m.getControllerNumber() - firstFaderCC;
        if (juce::isPositiveAndBelow (i, numFaders))
        {
            in.kind = Input::Kind::fader;
            in.index = i;
            in.value = (float) m.getControllerValue() / 127.0f;
        }
        return in;
    }

    if (! m.isNoteOnOrOff())
        return in;

    const int note = m.getNoteNumber();
    in.pressed = m.isNoteOn();
    if (note < gridSize * gridSize)
    {
        in.kind = Input::Kind::pad;
        in.row = gridSize - 1 - note / gridSize;
        in.column = note % gridSize;
    }
    else if (juce::isPositiveAndBelow (note - trackButtonNote (model, 0), gridSize))
    {
        in.kind = Input::Kind::trackButton;
        in.index = note - trackButtonNote (model, 0);
    }
    else if (juce::isPositiveAndBelow (note - sceneButtonNote (model, 0), gridSize))
    {
        in.kind = Input::Kind::sceneButton;
        in.index = note - sceneButtonNote (model, 0);
    }
    else if (note == shiftNote (model))
    {
        in.kind = Input::Kind::shift;
    }
    return in;
}

//==============================================================================
// État d'un pad, comme une case de la grille de Grid Play.
enum class Pad { off, clip, playing, playQueued, recording, recordQueued, armed };

// Palette du mk2 : quelques couleurs pures, choisies selon la couleur du clip.
struct PaletteEntry { int velocity; juce::Colour colour; };

inline int nearestMk2Colour (juce::Colour c)
{
    static const PaletteEntry entries[] = {
        { 5, juce::Colour (0xffff0000) }, { 9, juce::Colour (0xffff5400) }, { 13, juce::Colour (0xffffff00) },
        { 17, juce::Colour (0xff88ff00) }, { 21, juce::Colour (0xff00ff00) }, { 33, juce::Colour (0xff00ffff) },
        { 41, juce::Colour (0xff0088ff) }, { 45, juce::Colour (0xff0000ff) }, { 49, juce::Colour (0xff8800ff) },
        { 53, juce::Colour (0xffff00ff) }, { 57, juce::Colour (0xffff0055) }, { 3, juce::Colour (0xffffffff) },
    };
    if (c.getSaturation() < 0.15f)
        return 3;
    int best = 13;
    float bestDistance = 1.0e9f;
    for (const auto& e : entries)
    {
        const float dh = std::abs (c.getHue() - e.colour.getHue());
        const float d = juce::jmin (dh, 1.0f - dh);
        if (d < bestDistance) { bestDistance = d; best = e.velocity; }
    }
    return best;
}

// Le message qui allume un pad. « blinkOn » : phase du clignotement, gérée par Grid Play
// pour le mk2 (le mk1 clignote tout seul).
inline juce::MidiMessage padMessage (Model model, int note, Pad pad, juce::Colour clipColour, bool blinkOn)
{
    if (model == Model::mk1)
    {
        int v = 0;
        switch (pad)
        {
            case Pad::off:          v = 0; break;
            case Pad::clip:         v = 5; break;   // jaune
            case Pad::playing:      v = 1; break;   // vert
            case Pad::playQueued:   v = 2; break;   // vert clignotant
            case Pad::recording:    v = 3; break;   // rouge
            case Pad::recordQueued: v = 4; break;   // rouge clignotant
            case Pad::armed:        v = 0; break;
        }
        return juce::MidiMessage::noteOn (1, note, (juce::uint8) v);
    }

    int channel = 7, velocity = 0;   // canal 7 = 100 %
    switch (pad)
    {
        case Pad::off:          velocity = 0; break;
        case Pad::clip:         velocity = nearestMk2Colour (clipColour); channel = 3; break;   // couleur du clip, atténuée
        case Pad::playing:      velocity = 21; channel = blinkOn ? 7 : 4; break;                // vert qui pulse
        case Pad::playQueued:   velocity = blinkOn ? 21 : 0; break;
        case Pad::recording:    velocity = 5; channel = blinkOn ? 7 : 4; break;                 // rouge qui pulse
        case Pad::recordQueued: velocity = blinkOn ? 5 : 0; break;
        case Pad::armed:        velocity = 5; channel = 1; break;                                // rouge très faible
    }
    return juce::MidiMessage::noteOn (channel, note, (juce::uint8) velocity);
}

// Boutons à une couleur (bas et droite) : 0 éteint, 1 allumé, 2 clignotant.
inline juce::MidiMessage buttonMessage (int note, int state)
{
    return juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (0, 2, state));
}
} // namespace gridplay::apc
