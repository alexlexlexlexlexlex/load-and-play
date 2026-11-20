// Tests du moteur de Chop Chop, hors DAW.
// Utilisation : RenderTest

#include "../Source/ChopEngine.h"

#include <iostream>
#include <random>

using namespace chopchop;

namespace
{
constexpr double rate = 44100.0;
bool allOk = true;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "  OK    " : "  ECHEC ") << what << std::endl;
    allOk = allOk && condition;
}

std::string num (double v, int decimals = 2) { return juce::String (v, decimals).toStdString(); }

using Signal = std::vector<float>;

Signal sine (double freq, float amplitude, int length)
{
    Signal s ((size_t) length);
    for (int i = 0; i < length; ++i)
        s[(size_t) i] = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * freq * i / rate);
    return s;
}

Signal noise (float amplitude, int length, unsigned seed = 1)
{
    std::mt19937 gen (seed);
    std::uniform_real_distribution<float> dist (-amplitude, amplitude);
    Signal s ((size_t) length);
    for (auto& x : s) x = dist (gen);
    return s;
}

// Coups secs (sinus amorti) aux instants donnés, en secondes.
Signal hits (std::initializer_list<double> times, double freq, int length)
{
    Signal s ((size_t) length, 0.0f);
    for (double t : times)
        for (int i = 0; i < (int) (0.08 * rate); ++i)
        {
            const int at = (int) (t * rate) + i;
            if (at < length)
                s[(size_t) at] += 0.9f * (float) (std::exp (-i / (0.02 * rate)) * std::sin (juce::MathConstants<double>::twoPi * freq * i / rate));
        }
    return s;
}

struct Note { int at; int note; bool on; };

Signal render (const Params& p, const Signal& in, const Signal& side = {}, const std::vector<Note>& notes = {})
{
    ChopEngine engine;
    engine.prepare (rate);
    engine.setParams (p);
    Signal left = in, right = in;
    Transport t { 120.0, 0.0, true };
    const int length = (int) in.size(), block = 256;

    for (int pos = 0; pos < length;)
    {
        int n = std::min (block, length - pos);
        for (const auto& note : notes)
        {
            if (note.at == pos) { if (note.on) engine.noteOn (note.note, 1.0f); else engine.noteOff (note.note); }
            if (note.at > pos && note.at < pos + n) n = note.at - pos;
        }
        float* channels[] { left.data() + pos, right.data() + pos };
        const float* sides[] { side.empty() ? nullptr : side.data() + pos, side.empty() ? nullptr : side.data() + pos };
        engine.process (channels, 2, sides, side.empty() ? 0 : 2, n, t);
        t.ppq += n * t.bpm / 60.0 / rate;
        pos += n;
    }
    return left;
}

float rms (const Signal& s, int start, int end)
{
    double sum = 0.0;
    for (int i = start; i < end; ++i) sum += (double) s[(size_t) i] * s[(size_t) i];
    return (float) std::sqrt (sum / std::max (1, end - start));
}

float toneLevel (const Signal& s, double freq, int start, int end)
{
    const double w = juce::MathConstants<double>::twoPi * freq / rate;
    double re = 0.0, im = 0.0;
    for (int i = start; i < end; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (i - start) / (end - start));
        re += s[(size_t) i] * win * std::cos (w * i);
        im -= s[(size_t) i] * win * std::sin (w * i);
    }
    return (float) (4.0 * std::sqrt (re * re + im * im) / (end - start));
}

float maxDiff (const Signal& a, int aStart, const Signal& b, int bStart, int count, int step = 1)
{
    float m = 0.0f;
    for (int k = 0; k < count; ++k)
        m = std::max (m, std::abs (a[(size_t) (aStart + k)] - b[(size_t) (bStart + step * k)]));
    return m;
}

float db (float ratio) { return 20.0f * std::log10 (std::max (ratio, 1.0e-9f)); }

// À 120 BPM : une double-croche = 5512,5 échantillons ; le pas 4 commence à 22050.
// Le bout capturé finit sur l'échantillon où le pas commence (inclus).
constexpr int stepStart = 22050, sixteenth = 5512, sliceStart = stepStart - sixteenth + 1;

Params withPattern (const char* pattern)
{
    Params p;
    p.pattern = patternFromString (pattern);
    p.snap = false;
    return p;
}
} // namespace

int main()
{
    const int length = (int) (2.0 * rate);
    const auto voice = noise (0.3f, length, 3);

    std::cout << "Transparence" << std::endl;
    {
        const auto out = render (Params(), voice);
        check (maxDiff (out, 0, voice, 0, length) == 0.0f, "tout éteint : sortie identique à l'entrée");
    }

    std::cout << "Repeater" << std::endl;
    {
        const auto out = render (withPattern ("----R-----------"), voice);
        // Le bout répété : la double-croche qui précède le pas.
        check (maxDiff (out, stepStart + 300, voice, sliceStart + 300, sixteenth - 600) < 1.0e-5f,
               "REPEAT : rejoue la double-croche qui vient de passer");
        check (maxDiff (out, stepStart + sixteenth + 400, voice, stepStart + sixteenth + 400, 20000) == 0.0f,
               "pas suivant (THRU) : la voix revient telle quelle");

        const auto rev = render (withPattern ("----<-----------"), voice);
        check (maxDiff (rev, stepStart + 300, voice, stepStart - 300, sixteenth - 600, -1) < 1.0e-5f, "REVERSE : le même bout à l'envers");

        const auto roll = render (withPattern ("----LLLL--------"), voice);
        // Deuxième tour du roll : moitié moins long, repart du début du bout.
        check (maxDiff (roll, stepStart + sixteenth + 200, voice, sliceStart + 200, sixteenth / 2 - 400) < 1.0e-5f,
               "ROLL : chaque tour deux fois plus court");

        const auto tone = sine (440.0, 0.4f, length);
        const auto up = render (withPattern ("----UUUU--------"), tone);
        const auto down = render (withPattern ("----DDDD--------"), tone);
        const int a = stepStart + 500, b = stepStart + 4 * sixteenth - 500;
        check (toneLevel (up, 880.0, a, b) > 10.0f * toneLevel (up, 440.0, a, b), "+12 : la voix monte d'une octave (vitesse x2)");
        check (toneLevel (down, 220.0, a, b) > 10.0f * toneLevel (down, 440.0, a, b), "-12 : la voix descend d'une octave (vitesse /2)");

        const auto mute = render (withPattern ("----X-----------"), voice);
        check (rms (mute, stepStart + 300, stepStart + sixteenth - 10) < 1.0e-4f, "MUTE : silence pendant le pas");

        Params p = withPattern ("----RRRR--------");
        p.rise = 12.0f;
        const auto rise = render (p, tone);
        check (toneLevel (rise, 880.0, stepStart + sixteenth + 300, stepStart + 2 * sixteenth - 300) > toneLevel (rise, 440.0, stepStart + sixteenth + 300, stepStart + 2 * sixteenth - 300),
               "RISE +12 : la deuxième répétition sonne une octave plus haut");
    }

    std::cout << "Calage sur les syllabes" << std::endl;
    {
        // Des « syllabes » de 60 ms ; sans calage, le bout tombe entre deux syllabes.
        Signal syllables ((size_t) length, 0.0f);
        for (double t : { 0.12, 0.32, 0.72 })
            for (int i = 0; i < (int) (0.06 * rate); ++i)
                syllables[(size_t) ((int) (t * rate) + i)] = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 300.0 * i / rate);
        Params p = withPattern ("----R-----------");
        const auto plain = render (p, syllables);
        p.snap = true;
        const auto snapped = render (p, syllables);
        const float plainLevel = rms (plain, stepStart + 200, stepStart + 2000), snappedLevel = rms (snapped, stepStart + 200, stepStart + 2000);
        check (snappedLevel > 0.2f && plainLevel < 0.05f, "le bout démarre sur une syllabe (" + num (snappedLevel, 3) + " contre " + num (plainLevel, 3) + " sans calage)");
    }

    std::cout << "Déclenchement par les drums" << std::endl;
    {
        Params p = withPattern ("R---------------");
        p.trigger = Trigger::drums;
        p.hitLength = 2;   // 1/8 : 11025 échantillons
        const auto kick = hits ({ 0.5 }, 60.0, length);
        const auto out = render (p, voice, kick);
        check (maxDiff (out, 0, voice, 0, stepStart - 10) == 0.0f, "avant le coup : la voix passe telle quelle");
        // Le bout rejoué juste après le coup ressemble à la voix d'une double-croche plus tôt.
        float best = 1.0f;
        for (int lag = -64; lag <= 64; ++lag)
            best = std::min (best, maxDiff (out, stepStart + 400, voice, sliceStart + 400 + lag, 3000));
        check (best < 1.0e-4f, "sur le coup : la voix est hachée (bout de 1/16 rejoué)");
        check (maxDiff (out, stepStart + 11025 + 600, voice, stepStart + 11025 + 600, 10000) == 0.0f, "après 1/8 : la voix revient");
    }

    std::cout << "MIDI" << std::endl;
    {
        Params p = withPattern ("----------------");
        p.trigger = Trigger::midi;
        const auto tone = sine (440.0, 0.4f, length);
        const auto out = render (p, tone, {}, { { 22050, 72, true }, { 44100, 72, false } });
        check (toneLevel (out, 880.0, 23000, 43000) > 10.0f * toneLevel (out, 440.0, 23000, 43000), "note do 4 : le bout rejoué une octave au-dessus");
        check (maxDiff (out, 44100 + 400, tone, 44100 + 400, 20000) == 0.0f, "note relâchée : la voix revient");
    }

    std::cout << "Saleté, filtre, pompage, delay" << std::endl;
    {
        const auto tone = sine (220.0, 0.3f, length);
        Params p;
        p.driveDb = 24.0f;
        const auto warm = render (p, tone);
        const float third = db (toneLevel (warm, 660.0, 10000, 40000) / toneLevel (warm, 220.0, 10000, 40000));
        const float level = db (rms (warm, 10000, 40000) / rms (tone, 10000, 40000));
        check (third > -25.0f && std::abs (level) < 6.0f, "WARM 24 dB : harmoniques (" + num (third, 1) + " dB), niveau tenu (" + num (level, 1) + " dB)");

        p.dirt = Dirt::crush;
        const auto crush = render (p, tone);
        check (rms (crush, 10000, 40000) > 0.05f && maxDiff (crush, 10000, tone, 10000, 1000) > 0.01f, "CRUSH : son abîmé mais présent");

        Params dd;
        dd.drumDriveDb = 24.0f;
        const auto kick = hits ({ 0.5 }, 60.0, length);
        const auto breathing = render (dd, tone, kick);
        check (maxDiff (breathing, 4000, tone, 4000, 15000) == 0.0f
                   && db (toneLevel (breathing, 660.0, 22100, 23500) / toneLevel (breathing, 220.0, 22100, 23500)) > -30.0f,
               "DRUM DRIVE : propre entre les coups, saturé sur le coup");

        Params f;
        f.filter = FilterType::lowpass;
        f.cutoff = 500.0f;
        const auto high = sine (5000.0, 0.3f, length);
        check (db (rms (render (f, high), 10000, 40000) / rms (high, 10000, 40000)) < -20.0f, "filtre passe-bas 500 Hz : 5 kHz très atténué");

        Params pump;
        pump.pumpDb = 12.0f;
        const auto pumped = render (pump, tone, kick);
        check (db (rms (pumped, 22200, 23500) / rms (pumped, 4000, 15000)) < -8.0f, "pompage : la voix baisse sur le kick");

        Params d;
        d.echo = 1.0f;
        d.feedback = 0.0f;
        d.delayDivision = 1;   // 1/8 : 11025 échantillons
        Signal click ((size_t) length, 0.0f);
        click[1000] = 1.0f;
        const auto echoed = render (d, click);
        check (std::abs (echoed[(size_t) (1000 + 11025)]) > 0.3f, "delay 1/8 : l'écho arrive 250 ms plus tard");
    }

    std::cout << "Robustesse" << std::endl;
    {
        bool finite = true;
        std::mt19937 gen (9);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        const char* patterns[] { "RLUD<X-RRLLUUDD<", "LLLLLLLLLLLLLLLL", "X-X-X-X-UUUU<<<<" };
        for (int run = 0; run < 12; ++run)
        {
            Params p;
            p.trigger = (Trigger) (run % 3);
            p.pattern = patternFromString (patterns[run % 3]);
            p.stepRate = run % 3;
            p.size = run % 5;
            p.rise = 24.0f * u (gen) - 12.0f;
            p.decay = u (gen);
            p.chance = u (gen);
            p.pitch = 24.0f * u (gen) - 12.0f;
            p.dirt = (Dirt) (run % 3);
            p.driveDb = 40.0f * u (gen);
            p.drumDriveDb = 24.0f * u (gen);
            p.filter = (FilterType) (run % 4);
            p.cutoff = 20.0f + 18000.0f * u (gen);
            p.resonance = 0.5f + 9.0f * u (gen);
            p.filterMod = 2.0f * u (gen) - 1.0f;
            p.pumpDb = 24.0f * u (gen);
            p.echo = u (gen);
            p.feedback = 0.9f * u (gen);
            p.reverb = u (gen);
            p.snap = run % 2 == 0;
            const auto out = render (p, noise (1.0f, length / 2, (unsigned) run), hits ({ 0.1, 0.35, 0.6, 0.85 }, 80.0, length / 2),
                                     { { 3000, 64, true }, { 9000, 76, true }, { 15000, 64, false }, { 20000, 76, false } });
            for (auto x : out) finite = finite && std::isfinite (x) && std::abs (x) < 8.0f;
        }
        check (finite, "réglages au hasard, bruit à pleine échelle : signal sain");
    }

    std::cout << (allOk ? "Tout est bon." : "Des tests ont échoué.") << std::endl;
    return allOk ? 0 : 1;
}
