#pragma once

// Sons et motifs d'usine de LP-303, créés pour ce plugin.
//
// Un preset ne règle que le son (il part des réglages par défaut et ne liste
// que ce qui change) : les motifs du séquenceur restent ceux de l'utilisateur.
// Les 8 motifs d'usine remplissent la banque d'un plugin tout neuf.

#include "Sequencer.h"

#include <cstring>
#include <iterator>
#include <utility>
#include <vector>

namespace lp303
{

struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // identifiant de paramètre, valeur réelle
};

inline const std::vector<Preset>& factoryPresets()
{
    // wave : 0 = dents de scie, 1 = carré. slide en secondes.
    static const std::vector<Preset> presets {
        { "Init", {} },
        { "Classic Squelch", { { "cutoff", 3.0f }, { "resonance", 8.0f }, { "envMod", 6.0f }, { "decay", 4.0f },
                               { "accent", 7.0f } } },
        { "Rubber Square", { { "wave", 1 }, { "cutoff", 3.5f }, { "resonance", 6.5f }, { "envMod", 5.0f },
                             { "decay", 3.0f }, { "accent", 6.0f } } },
        { "Deep Sub", { { "cutoff", 1.5f }, { "resonance", 3.0f }, { "envMod", 2.5f }, { "decay", 6.0f },
                        { "accent", 3.0f }, { "volume", 8.0f } } },
        { "Screamer", { { "cutoff", 4.5f }, { "resonance", 9.5f }, { "envMod", 8.0f }, { "decay", 3.5f },
                        { "accent", 9.0f }, { "drive", 6.0f } } },
        { "Distorted Acid", { { "cutoff", 3.0f }, { "resonance", 8.5f }, { "envMod", 7.0f }, { "decay", 5.0f },
                              { "accent", 8.0f }, { "drive", 8.0f }, { "volume", 6.0f } } },
        { "Wah Tone", { { "wave", 1 }, { "cutoff", 2.0f }, { "resonance", 9.0f }, { "envMod", 9.0f },
                        { "decay", 7.0f }, { "accent", 8.0f } } },
        { "Plucky", { { "cutoff", 2.5f }, { "resonance", 5.0f }, { "envMod", 7.0f }, { "decay", 1.5f },
                      { "accent", 5.0f } } },
        { "Long Slide", { { "cutoff", 4.0f }, { "resonance", 7.0f }, { "envMod", 4.0f }, { "decay", 6.0f },
                          { "accent", 6.0f }, { "slide", 0.25f } } },
        { "Acid Lead", { { "wave", 1 }, { "tune", 12.0f }, { "cutoff", 5.0f }, { "resonance", 7.0f },
                         { "envMod", 5.0f }, { "decay", 4.0f }, { "accent", 7.0f }, { "drive", 4.0f } } },
    };
    return presets;
}

//==============================================================================
// Motifs d'usine, écrits note par note : 16 jetons séparés par des espaces.
//   C C# D D# E F F# G G# A A# B, et C^ pour le do aigu ;  .  = silence
//   suivis de : u = octave haut, d = octave bas, a = accent, s = slide.
struct FactoryPattern
{
    const char* name;
    int length;
    const char* steps;
};

inline const std::vector<FactoryPattern>& factoryPatterns()
{
    static const std::vector<FactoryPattern> patterns {
        { "Acid Roll",    16, "C Ca Cu C . D#s F Cu Ca . A#d C Gs Cu C D#a" },
        { "Squelch",      16, "Cd Cs C Cua . Cd Gd A#d Cd Cs D#a Cd . F Fs Ga" },
        { "Slide Walk",   16, "C Ds D#s F Gs G#a G Fs D# Cs Cu A#s G F D#a D" },
        { "Hypno 12",     12, "C Cu . C Cs Cu C . D#a C Cu A#d . . . ." },
        { "Offbeat Pump", 16, ". C . Ca . C . Cu . C . Ca . D# . F" },
        { "Minor Run",    16, "C D# F G A#s Cu A#s G F D# C A#d Gd A#d Ca ." },
        { "Bounce",       16, "Cd C Cd Cu Cd Ca Cd D# Cd F Cd Fs F#s G Cd A#a" },
        { "Sparse Wow",    8, "Ca . . Cs Cua . D#s Ca . . . . . . . ." },
    };
    return patterns;
}

// Lit un jeton (« D#ua ») ; renvoie un silence si le jeton ne commence pas par une note.
inline Step parseStepToken (const juce::String& token)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    Step step;
    if (token.isEmpty() || token == ".")
    {
        step.gate = false;
        return step;
    }

    int length = 0;
    if (token.startsWith ("C^"))
    {
        step.note = 12;
        length = 2;
    }
    else
    {
        for (int n = 11; n >= 0; --n)        // les dièses d'abord
        {
            if (token.startsWith (names[n]))
            {
                step.note = n;
                length = (int) std::strlen (names[n]);
                break;
            }
        }
        if (length == 0)
        {
            step.gate = false;
            return step;
        }
    }

    const auto modifiers = token.substring (length);
    if (modifiers.containsChar ('u')) step.octave = 1;
    if (modifiers.containsChar ('d')) step.octave = -1;
    step.accent = modifiers.containsChar ('a');
    step.slide = modifiers.containsChar ('s');
    return step;
}

inline void loadFactoryPatterns (PatternBank& bank)
{
    const auto& patterns = factoryPatterns();
    for (int p = 0; p < numPatterns && p < (int) patterns.size(); ++p)
    {
        const auto tokens = juce::StringArray::fromTokens (patterns[(size_t) p].steps, " ", "");
        for (int s = 0; s < numSteps; ++s)
            bank.set (p, s, parseStepToken (tokens[s]));
        bank.setLength (p, patterns[(size_t) p].length);
    }
}

// Motif « acid » au hasard : surtout la tonique, quelques notes d'une gamme
// mineure, des octaves, des accents, des slides et des silences.
inline void randomPattern (PatternBank& bank, int pattern, juce::Random& random)
{
    static const int scale[] = { 0, 0, 0, 0, 3, 5, 7, 7, 10, 12 };
    for (int s = 0; s < numSteps; ++s)
    {
        Step step;
        step.note = scale[random.nextInt ((int) std::size (scale))];
        const float r = random.nextFloat();
        step.octave = r < 0.2f ? 1 : (r < 0.3f ? -1 : 0);
        step.accent = random.nextFloat() < 0.25f;
        step.slide = random.nextFloat() < 0.2f;
        step.gate = random.nextFloat() > 0.15f;
        bank.set (pattern, s, step);
    }
}

} // namespace lp303
