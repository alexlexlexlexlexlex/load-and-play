// Opérations de l'éditeur de notes, sans interface, pour pouvoir les tester :
// grille, positions au format de Cubase (mesure.temps.double-croche.tick),
// noms des notes (do 3 = 60, comme Cubase), quantification, transposition,
// découpe, collage, legato, duplication.
#pragma once

#include "Model.h"

#include <set>

namespace gridplay::notes
{
using Selection = std::set<int>;   // indices de notes sélectionnées

//==============================================================================
// Grille (en temps, une noire = 1)
struct GridValue { const char* name; double beats; };

inline const std::vector<GridValue>& gridValues()
{
    static const std::vector<GridValue> values {
        { "1/1", 4.0 }, { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 }, { "1/64", 0.0625 },
        { "1/2 T", 4.0 / 3.0 }, { "1/4 T", 2.0 / 3.0 }, { "1/8 T", 1.0 / 3.0 }, { "1/16 T", 1.0 / 6.0 }, { "1/32 T", 1.0 / 12.0 },
    };
    return values;
}

inline double snapNearest (double beat, double grid) { return grid > 0.0 ? std::round (beat / grid) * grid : beat; }
inline double snapFloor (double beat, double grid)   { return grid > 0.0 ? std::floor (beat / grid + 1.0e-9) * grid : beat; }

//==============================================================================
// Positions et longueurs comme dans Cubase : 480 ticks par noire, 120 par double-croche.
constexpr int ticksPerBeat = 480;

inline juce::String formatPosition (double beat)
{
    const long long ticks = juce::jmax (0LL, (long long) std::llround (beat * ticksPerBeat));
    const long long perBar = (long long) ticksPerBeat * beatsPerBar;
    const long long bar = ticks / perBar, inBar = ticks % perBar;
    const long long b = inBar / ticksPerBeat, inBeat = inBar % ticksPerBeat;
    return juce::String (bar + 1) + "." + juce::String (b + 1) + "." + juce::String (inBeat / 120 + 1) + "." + juce::String (inBeat % 120);
}

inline juce::String formatLength (double beats)
{
    const long long ticks = juce::jmax (0LL, (long long) std::llround (beats * ticksPerBeat));
    const long long perBar = (long long) ticksPerBeat * beatsPerBar;
    const long long bar = ticks / perBar, inBar = ticks % perBar;
    const long long b = inBar / ticksPerBeat, inBeat = inBar % ticksPerBeat;
    return juce::String (bar) + "." + juce::String (b) + "." + juce::String (inBeat / 120) + "." + juce::String (inBeat % 120);
}

// « 2.3.1.0 » (position, champs à partir de 1) ou « 0.1.0.0 » (longueur, à partir de 0).
// Les champs manquants valent leur minimum. Renvoie -1 si le texte n'est pas lisible.
inline double parsePosition (const juce::String& text, bool isLength)
{
    auto parts = juce::StringArray::fromTokens (text.trim(), ".:, ", {});
    parts.removeEmptyStrings();
    if (parts.isEmpty() || parts.size() > 4)
        return -1.0;
    const int base = isLength ? 0 : 1;
    double fields[4] = { (double) base, (double) base, (double) base, 0.0 };
    for (int i = 0; i < parts.size(); ++i)
    {
        if (! parts[i].containsOnly ("0123456789"))
            return -1.0;
        fields[i] = parts[i].getDoubleValue();
    }
    return (fields[0] - base) * beatsPerBar + (fields[1] - base) + (fields[2] - base) * 0.25 + fields[3] / ticksPerBeat;
}

inline juce::String pitchName (int pitch)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[pitch % 12]) + juce::String (pitch / 12 - 2);
}

// « C3 », « f#2 », « Db4 », « D-1 », ou un numéro MIDI. -1 si illisible.
inline int parsePitch (const juce::String& text)
{
    auto t = text.trim().toUpperCase();
    if (t.containsOnly ("0123456789") && t.isNotEmpty())
        return juce::jlimit (0, 127, t.getIntValue());
    static const juce::String letters ("C D EF G A B");
    if (t.isEmpty() || ! juce::String ("CDEFGAB").containsChar (t[0]))
        return -1;
    int semitone = letters.indexOfChar (t[0]);
    int i = 1;
    if (t[i] == '#') { ++semitone; ++i; }
    else if (t[i] == 'B' && t.length() > 2) { --semitone; ++i; }
    const auto octaveText = t.substring (i);
    if (octaveText.isEmpty() || ! octaveText.containsOnly ("-0123456789"))
        return -1;
    const int pitch = (octaveText.getIntValue() + 2) * 12 + semitone;
    return juce::isPositiveAndBelow (pitch, 128) ? pitch : -1;
}

//==============================================================================
// Drum map General MIDI (35 à 81), noms en français : ce que montre l'éditeur en mode batterie.
inline juce::String drumName (int pitch)
{
    static const char* names[] = {
        "Grosse caisse 2", "Grosse caisse", "Rimshot", "Caisse claire", "Clap", "Caisse claire 2", "Tom basse grave",
        "Charleston fermé", "Tom basse", "Charleston pied", "Tom grave", "Charleston ouvert", "Tom médium grave",
        "Tom médium aigu", "Crash 1", "Tom aigu", "Ride 1", "Chinese", "Cloche de ride", "Tambourin", "Splash",
        "Cloche", "Crash 2", "Vibraslap", "Ride 2", "Bongo aigu", "Bongo grave", "Conga aigu étouffé", "Conga aigu",
        "Conga grave", "Timbale aiguë", "Timbale grave", "Agogo aigu", "Agogo grave", "Cabasa", "Maracas",
        "Sifflet court", "Sifflet long", "Guiro court", "Guiro long", "Claves", "Wood block aigu", "Wood block grave",
        "Cuica étouffée", "Cuica ouverte", "Triangle étouffé", "Triangle ouvert" };
    if (pitch >= 35 && pitch <= 81)
        return juce::String::fromUTF8 (names[pitch - 35]);
    return {};
}

// Lignes de l'éditeur en mode batterie, de haut en bas : la drum map, puis les autres notes du clip.
inline std::vector<int> drumRows (const std::vector<Note>& notes)
{
    std::vector<int> rows;
    for (int p = 35; p <= 81; ++p)
        rows.push_back (p);
    std::set<int> extra;
    for (const auto& n : notes)
        if (n.pitch < 35 || n.pitch > 81)
            extra.insert (n.pitch);
    rows.insert (rows.end(), extra.begin(), extra.end());
    return rows;
}

//==============================================================================
// Couleur selon la vélocité, comme Cubase : bleu (doux) -> vert -> rouge (fort).
inline juce::Colour velocityColour (int velocity)
{
    const float t = juce::jlimit (0.0f, 1.0f, (float) (velocity - 1) / 126.0f);
    return juce::Colour::fromHSV (0.62f * (1.0f - t), 0.72f, 0.95f, 1.0f);
}

//==============================================================================
// Les fonctions qui suivent travaillent sur les notes sélectionnées (ou toutes si la
// sélection est vide, comme la quantification de Cubase) et renvoient la nouvelle sélection.
inline Selection allOrSelected (const std::vector<Note>& notes, const Selection& selection)
{
    if (! selection.empty())
        return selection;
    Selection all;
    for (int i = 0; i < (int) notes.size(); ++i)
        all.insert (i);
    return all;
}

// Réglages de quantification, comme le panneau de Cubase.
struct QuantizeSettings
{
    double grid = 0.25;
    double swing = 0.0;      // 0..1 : retarde un pas de grille sur deux (1 = d'un demi-pas)
    double strength = 1.0;   // 0..1 : rapproche de la grille sans y coller (« force » / iterative)
    bool lengths = false;    // quantifie aussi les fins de notes
};

// Point de grille le plus proche, avec le swing sur les pas impairs.
inline double quantizeTarget (double beat, const QuantizeSettings& q)
{
    if (q.grid <= 0.0)
        return beat;
    const double step = std::round (beat / q.grid);
    double target = step * q.grid;
    if (q.swing > 0.0 && std::fmod (std::abs (step), 2.0) > 0.5)
        target += q.swing * q.grid * 0.5;
    return target;
}

inline void quantize (std::vector<Note>& notes, const Selection& selection, const QuantizeSettings& q)
{
    for (int i : allOrSelected (notes, selection))
    {
        auto& n = notes[(size_t) i];
        const double end = n.start + n.length;
        n.start = juce::jmax (0.0, n.start + (quantizeTarget (n.start, q) - n.start) * q.strength);
        if (q.lengths)
        {
            const double newEnd = end + (snapNearest (end, q.grid) - end) * q.strength;
            n.length = juce::jmax (q.grid * 0.5, newEnd - n.start);
        }
        // Sinon la longueur ne change pas : la note se déplace en entier, comme dans Cubase.
    }
}

inline void quantizeStarts (std::vector<Note>& notes, const Selection& selection, double grid)
{
    for (int i : allOrSelected (notes, selection))
        notes[(size_t) i].start = juce::jmax (0.0, snapNearest (notes[(size_t) i].start, grid));
}

inline void quantizeLengths (std::vector<Note>& notes, const Selection& selection, double grid)
{
    for (int i : allOrSelected (notes, selection))
        notes[(size_t) i].length = juce::jmax (grid, snapNearest (notes[(size_t) i].length, grid));
}

inline void transpose (std::vector<Note>& notes, const Selection& selection, int semitones)
{
    // Rien ne bouge si une note sortirait du clavier : on garde les accords intacts.
    for (int i : selection)
        if (! juce::isPositiveAndBelow (notes[(size_t) i].pitch + semitones, 128))
            return;
    for (int i : selection)
        notes[(size_t) i].pitch += semitones;
}

inline void nudge (std::vector<Note>& notes, const Selection& selection, double beats)
{
    double earliest = 1.0e9;
    for (int i : selection)
        earliest = juce::jmin (earliest, notes[(size_t) i].start);
    const double shift = juce::jmax (-earliest, beats);
    for (int i : selection)
        notes[(size_t) i].start += shift;
}

inline void resize (std::vector<Note>& notes, const Selection& selection, double beats, double minimum)
{
    for (int i : selection)
        notes[(size_t) i].length = juce::jmax (minimum, notes[(size_t) i].length + beats);
}

// Coupe les notes sélectionnées qui traversent « at » (ou seulement « index » si elle n'est pas sélectionnée).
inline Selection splitAt (std::vector<Note>& notes, const Selection& selection, int index, double at)
{
    Selection targets = selection.count (index) > 0 ? selection : Selection { index };
    Selection result;
    for (int i : targets)
    {
        auto& n = notes[(size_t) i];
        if (at <= n.start + 1.0e-9 || at >= n.start + n.length - 1.0e-9)
        {
            result.insert (i);
            continue;
        }
        Note second = n;
        second.start = at;
        second.length = n.start + n.length - at;
        n.length = at - n.start;
        notes.push_back (second);
        result.insert (i);
        result.insert ((int) notes.size() - 1);
    }
    return result;
}

// Colle une note à la suivante de même hauteur (outil colle de Cubase).
inline int glue (std::vector<Note>& notes, int index)
{
    if (! juce::isPositiveAndBelow (index, (int) notes.size()))
        return -1;
    const auto& n = notes[(size_t) index];
    int next = -1;
    for (int i = 0; i < (int) notes.size(); ++i)
        if (i != index && notes[(size_t) i].pitch == n.pitch && notes[(size_t) i].start > n.start + 1.0e-9
            && (next < 0 || notes[(size_t) i].start < notes[(size_t) next].start))
            next = i;
    if (next < 0)
        return index;
    notes[(size_t) index].length = notes[(size_t) next].start + notes[(size_t) next].length - notes[(size_t) index].start;
    notes.erase (notes.begin() + next);
    return next < index ? index - 1 : index;
}

// Legato : chaque note dure jusqu'au début de la note suivante (quelle que soit sa hauteur).
inline void legato (std::vector<Note>& notes, const Selection& selection, double clipLength)
{
    for (int i : allOrSelected (notes, selection))
    {
        auto& n = notes[(size_t) i];
        double next = clipLength;
        for (const auto& other : notes)
            if (other.start > n.start + 1.0e-9)
                next = juce::jmin (next, other.start);
        if (next > n.start)
            n.length = next - n.start;
    }
}

// Duplique la sélection juste après elle-même (Cmd D de Cubase). Renvoie les copies.
inline Selection duplicate (std::vector<Note>& notes, const Selection& selection)
{
    if (selection.empty())
        return {};
    double first = 1.0e9, last = 0.0;
    for (int i : selection)
    {
        first = juce::jmin (first, notes[(size_t) i].start);
        last = juce::jmax (last, notes[(size_t) i].start + notes[(size_t) i].length);
    }
    const double shift = last - first;
    Selection copies;
    for (int i : selection)
    {
        Note copy = notes[(size_t) i];
        copy.start += shift;
        notes.push_back (copy);
        copies.insert ((int) notes.size() - 1);
    }
    return copies;
}

inline void erase (std::vector<Note>& notes, const Selection& selection)
{
    for (auto it = selection.rbegin(); it != selection.rend(); ++it)
        if (juce::isPositiveAndBelow (*it, (int) notes.size()))
            notes.erase (notes.begin() + *it);
}
} // namespace gridplay::notes
