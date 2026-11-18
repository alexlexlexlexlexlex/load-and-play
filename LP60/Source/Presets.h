#pragma once

// Sons d'usine de LP-60, créés pour ce plugin. Chaque preset part des
// réglages par défaut et ne liste que ce qui change.

#include <utility>
#include <vector>

namespace lp60
{

struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // identifiant de paramètre, valeur réelle
};

inline const std::vector<Preset>& factoryPresets()
{
    // Choix : range 0 = 16', 1 = 8', 2 = 4' ; pwmMode 0 = LFO, 1 = MAN, 2 = ENV ;
    // vcaMode 0 = ENV, 1 = GATE ; arpMode 0 = UP, 1 = U&D, 2 = DOWN ; arpRange 0..2 = 1..3 oct.
    static const std::vector<Preset> presets {
        { "Init", {} },
        { "Warm Pad", { { "saw", 1 }, { "pulse", 1 }, { "pwm", 5 }, { "pwmMode", 0 }, { "lfoRate", 3 },
                        { "cutoff", 5.5f }, { "resonance", 1.5f }, { "vcfEnv", 2 }, { "vcfKybd", 4 },
                        { "attack", 6 }, { "decay", 7 }, { "sustain", 8 }, { "release", 6.5f }, { "chorus2", 1 } } },
        { "Strings", { { "saw", 1 }, { "cutoff", 6.5f }, { "vcfKybd", 5 }, { "hpf", 1 },
                       { "attack", 4.5f }, { "decay", 6 }, { "sustain", 9 }, { "release", 6 }, { "chorus1", 1 } } },
        { "Brass", { { "saw", 1 }, { "cutoff", 3.5f }, { "resonance", 2 }, { "vcfEnv", 6 }, { "vcfKybd", 5 },
                     { "attack", 3 }, { "decay", 5.5f }, { "sustain", 6 }, { "release", 3.5f } } },
        { "Bass", { { "saw", 0 }, { "pulse", 1 }, { "subOn", 1 }, { "sub", 8 }, { "range", 0 }, { "cutoff", 3 }, { "resonance", 3 },
                    { "vcfEnv", 6 }, { "vcfKybd", 3 }, { "decay", 4.5f }, { "sustain", 3 }, { "release", 2 } } },
        { "Sub Bass", { { "saw", 0 }, { "pulse", 1 }, { "subOn", 1 }, { "sub", 10 }, { "range", 0 }, { "cutoff", 2.5f },
                        { "release", 2 } } },
        { "Organ", { { "saw", 0 }, { "pulse", 1 }, { "subOn", 1 }, { "sub", 6 }, { "vcaMode", 1 }, { "cutoff", 7 }, { "hpf", 1 },
                     { "chorus1", 1 }, { "chorus2", 1 } } },
        { "Pluck Arp", { { "saw", 1 }, { "cutoff", 3.5f }, { "resonance", 4 }, { "vcfEnv", 6 }, { "decay", 4 },
                         { "sustain", 0 }, { "release", 4 }, { "arpOn", 1 }, { "arpMode", 1 }, { "arpRange", 1 },
                         { "arpRate", 5.5f }, { "chorus2", 1 } } },
        { "Wide Lead", { { "saw", 1 }, { "pulse", 1 }, { "pwm", 6 }, { "pwmMode", 0 }, { "lfoRate", 6 },
                         { "lfoDelay", 6 }, { "dcoLfo", 1.5f }, { "cutoff", 6 }, { "resonance", 2.5f },
                         { "vcfEnv", 3 }, { "decay", 5 }, { "sustain", 7 }, { "release", 3 }, { "chorus2", 1 } } },
        { "Sweep Pad", { { "saw", 1 }, { "cutoff", 4 }, { "resonance", 5 }, { "vcfLfo", 4 }, { "lfoRate", 1 },
                         { "attack", 5 }, { "release", 7 }, { "chorus1", 1 } } },
        { "Bells", { { "saw", 0 }, { "pulse", 1 }, { "pwm", 7 }, { "cutoff", 5 }, { "resonance", 7 }, { "vcfKybd", 10 },
                     { "vcfEnv", 4 }, { "decay", 7 }, { "sustain", 0 }, { "release", 7 }, { "chorus2", 1 } } },
        { "Noise FX", { { "saw", 0 }, { "noise", 8 }, { "cutoff", 6 }, { "resonance", 6 }, { "vcfLfo", 5 },
                        { "lfoRate", 2 }, { "attack", 7 }, { "release", 8 }, { "hpf", 2 }, { "chorus1", 1 } } },
    };
    return presets;
}

} // namespace lp60
