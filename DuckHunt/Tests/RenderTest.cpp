// Tests du moteur de Duck Hunt, hors DAW.
// Utilisation : RenderTest [dossier où écrire des exemples WAV]

#include "../Source/DuckEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <iostream>
#include <random>

using namespace duckhunt;

namespace
{
constexpr double rate = 44100.0;
bool allOk = true;

void check (bool condition, const juce::String& what)
{
    // Les textes viennent de littéraux UTF-8 lus octet par octet : on rend les octets tels quels.
    std::string text;
    for (auto c : what) text += (char) c;
    std::cout << (condition ? "  OK    " : "  ECHEC ") << text << std::endl;
    allOk = allOk && condition;
}

using Signal = std::vector<float>;

Signal sine (double freq, float amplitude, int length, int start = 0, int end = -1)
{
    Signal s ((size_t) length, 0.0f);
    for (int i = start; i < (end < 0 ? length : end); ++i)
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

Signal add (Signal a, const Signal& b) { for (size_t i = 0; i < a.size(); ++i) a[i] += b[i]; return a; }

struct Render
{
    Signal left, right;
    int latency = 0;
};

// Rend la piste (stéréo identique) avec la référence (vide = pas de sidechain).
Render render (const Params& p, const Signal& main, const Signal& side,
               const std::vector<std::pair<int, float>>& notes = {}, Transport transport = {})
{
    DuckEngine engine;
    engine.prepare (rate);
    engine.setParams (p);

    Render r;
    r.latency = DuckEngine::latencyFor (p, rate, engine.spectralSize());
    r.left = main;
    r.right = main;
    const int length = (int) main.size(), block = 512;
    Signal sideCopy = side.empty() ? Signal () : side;

    for (int pos = 0; pos < length;)
    {
        int n = std::min (block, length - pos);
        for (const auto& [at, velocity] : notes)
        {
            if (at == pos) engine.noteOn (velocity);
            if (at > pos && at < pos + n) n = at - pos;
        }

        float* channels[] { r.left.data() + pos, r.right.data() + pos };
        const float* sides[] { side.empty() ? nullptr : sideCopy.data() + pos, side.empty() ? nullptr : sideCopy.data() + pos };
        engine.process (channels, 2, sides, side.empty() ? 0 : 2, n, transport);
        transport.ppq += n * transport.bpm / 60.0 / rate;
        pos += n;
    }
    return r;
}

float rms (const Signal& s, int start, int end)
{
    double sum = 0.0;
    for (int i = start; i < end; ++i) sum += (double) s[(size_t) i] * s[(size_t) i];
    return (float) std::sqrt (sum / std::max (1, end - start));
}

// Amplitude d'une fréquence (Goertzel) sur une portion du signal.
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

float db (float ratio) { return 20.0f * std::log10 (std::max (ratio, 1.0e-9f)); }

void writeWav (const juce::File& file, const Signal& left, const Signal& right)
{
    juce::AudioBuffer<float> buffer (2, (int) left.size());
    buffer.copyFrom (0, 0, left.data(), (int) left.size());
    buffer.copyFrom (1, 0, right.data(), (int) right.size());
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), rate, 2, 24, {}, 0)))
    {
        stream.release();
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
}
} // namespace

int main (int argc, char* argv[])
{
    const int length = (int) (2.0 * rate);
    const auto music = add (noise (0.25f, length, 7), sine (440.0, 0.3f, length));

    std::cout << "Transparence (référence muette)" << std::endl;
    for (auto mode : { Mode::fullBand, Mode::threeBands, Mode::spectral })
    {
        Params p;
        p.mode = mode;
        const auto r = render (p, music, Signal ((size_t) length, 0.0f));
        const juce::String name = mode == Mode::fullBand ? "pleine bande" : (mode == Mode::threeBands ? "3 bandes" : "spectral");

        if (mode == Mode::threeBands)
        {
            // Filtres de Linkwitz-Riley : même niveau à toutes les fréquences (la phase tourne).
            float worst = 0.0f;
            for (double f : { 60.0, 200.0, 700.0, 2500.0, 8000.0 })
            {
                const auto in = sine (f, 0.5f, length);
                const auto out = render (p, in, Signal ((size_t) length, 0.0f));
                worst = std::max (worst, std::abs (db (rms (out.left, 22050, length) / rms (in, 22050, length))));
            }
            check (worst < 0.05f, name + " : même niveau de 60 Hz à 8 kHz (écart " + juce::String (worst, 3) + " dB)");
        }
        else
        {
            float maxDiff = 0.0f;
            for (int i = r.latency + 4096; i < length; ++i)
                maxDiff = std::max (maxDiff, std::abs (r.left[(size_t) i] - music[(size_t) (i - r.latency)]));
            check (maxDiff < 1.0e-4f, name + " : sortie identique à l'entrée, latence " + juce::String (r.latency)
                                    + " éch. (écart " + juce::String (maxDiff, 6) + ")");
        }
    }

    std::cout << "Duck par la référence" << std::endl;
    {
        Params p;   // pleine bande, -12 dB, seuil -30, plage 12
        const auto main = sine (440.0, 0.5f, length);
        const auto kick = sine (60.0, 0.9f, length, 22050, 44100);   // de 0,5 à 1 s
        const auto r = render (p, main, kick);
        const float before = rms (r.left, 4410, 17640), during = rms (r.left, 30000, 42000), after = rms (r.left, 70000, length);
        check (std::abs (db (during / before) + 12.0f) < 0.3f, "pendant la référence : -12 dB (" + juce::String (db (during / before), 2) + " dB)");
        check (std::abs (db (after / before)) < 0.05f, "après le relâchement : remonte à 0 dB (" + juce::String (db (after / before), 2) + " dB)");

        // Pas de compression : la baisse ne dépend que de la référence, pas du niveau de la piste.
        const auto loud = render (p, sine (440.0, 0.05f, length), kick);
        check (std::abs (db (rms (loud.left, 30000, 42000) / rms (loud.left, 4410, 17640)) - db (during / before)) < 0.05f,
               "même baisse quelle que soit le niveau de la piste");

        // Seuil et plage : référence à -24 dB, à mi-chemin entre -30 et -18 : -6 dB.
        const auto half = render (p, main, sine (60.0, dbToGain (-24.0f), length, 22050, 44100));
        const float halfDb = db (rms (half.left, 30000, 42000) / rms (half.left, 4410, 17640));
        check (std::abs (halfDb + 6.0f) < 0.6f, "référence à -24 dB : baisse de moitié (" + juce::String (halfDb, 2) + " dB)");

        const auto quiet = render (p, main, sine (60.0, dbToGain (-36.0f), length, 22050, 44100));
        check (std::abs (db (rms (quiet.left, 30000, 42000) / rms (quiet.left, 4410, 17640))) < 0.05f, "référence sous le seuil : rien ne bouge");

        // Inversion : baisse quand la référence se tait.
        p.invert = true;
        const auto inv = render (p, main, kick);
        check (std::abs (db (rms (inv.left, 4410, 17640) / 0.3536f) + 12.0f) < 0.3f && std::abs (db (rms (inv.left, 30000, 42000) / 0.3536f)) < 0.3f,
               "inversé : -12 dB dans le silence, 0 dB pendant la référence");
    }

    std::cout << "3 bandes" << std::endl;
    {
        Params p;
        p.mode = Mode::threeBands;
        p.depthDb = 18.0f;
        const auto main = add (sine (70.0, 0.4f, length), sine (6000.0, 0.4f, length));
        const auto bass = sine (80.0, 0.9f, length, 22050, length);
        const auto r = render (p, main, bass);
        const float lowDb = db (toneLevel (r.left, 70.0, 50000, length) / 0.4f);
        const float highDb = db (toneLevel (r.left, 6000.0, 50000, length) / 0.4f);
        check (lowDb < -15.0f && std::abs (highDb) < 0.5f, "référence grave : graves -" + juce::String (-lowDb, 1)
                                                            + " dB, aigus " + juce::String (highDb, 2) + " dB");
        p.bandAmount = { 0.0f, 1.0f, 1.0f };
        const auto off = render (p, main, bass);
        check (std::abs (db (toneLevel (off.left, 70.0, 50000, length) / 0.4f)) < 0.5f, "bande grave à 0 % : ne bouge plus");
    }

    std::cout << "Spectral" << std::endl;
    {
        Params p;
        p.mode = Mode::spectral;
        p.depthDb = 18.0f;
        p.smoothing = 0.2f;
        const auto main = add (sine (200.0, 0.3f, length), sine (5000.0, 0.3f, length));
        const auto voice = sine (200.0, 0.8f, length);
        const auto r = render (p, main, voice);
        const int from = r.latency + 22050;
        const float lowDb = db (toneLevel (r.left, 200.0, from, length) / 0.3f);
        const float highDb = db (toneLevel (r.left, 5000.0, from, length) / 0.3f);
        check (lowDb < -15.0f && std::abs (highDb) < 1.0f, "référence à 200 Hz : 200 Hz -" + juce::String (-lowDb, 1)
                                                            + " dB, 5 kHz " + juce::String (highDb, 2) + " dB");
        p.focusLow = 1000.0f;
        const auto focused = render (p, main, voice);
        check (std::abs (db (toneLevel (focused.left, 200.0, from, length) / 0.3f)) < 1.0f, "zone 1 kHz à 20 kHz : 200 Hz épargné");

        if (argc >= 2)
        {
            const juce::File dir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
            dir.createDirectory();
            writeWav (dir.getChildFile ("spectral.wav"), r.left, r.right);
        }
    }

    std::cout << "Déclenchements" << std::endl;
    {
        Params p;
        p.trigger = Trigger::sync;
        p.depthDb = 24.0f;
        p.attackMs = 1.0f;
        p.holdMs = 0.0f;
        p.releaseMs = 200.0f;
        Transport t;
        t.bpm = 120.0;
        t.playing = true;
        const auto steady = sine (440.0, 0.5f, length);
        const auto r = render (p, steady, {}, {}, t);
        int dips = 0;
        for (int beat = 0; beat < 4; ++beat)
        {
            const int at = (int) (beat * 0.5 * rate);
            dips += rms (r.left, at + 200, at + 600) < 0.5f * rms (r.left, at + 15000, at + 20000) ? 1 : 0;
        }
        check (dips == 4, "tempo 120, 1/4 : une baisse par temps (" + juce::String (dips) + "/4)");

        p.trigger = Trigger::midi;
        const auto m = render (p, steady, {}, { { 22050, 1.0f }, { 66150, 0.5f } });
        const float full = db (rms (m.left, 22200, 22600) / rms (m.left, 4410, 17640));
        const float half = db (rms (m.left, 66300, 66700) / rms (m.left, 4410, 17640));
        check (std::abs (full + 24.0f) < 1.5f && std::abs (half + 12.0f) < 1.5f,
               "notes MIDI : vélocité 127 -> " + juce::String (full, 1) + " dB, vélocité 64 -> " + juce::String (half, 1) + " dB");
    }

    std::cout << "Anticipation" << std::endl;
    {
        // Même signal en piste et en référence : un coup sec. Sans anticipation, l'attaque
        // laisse passer le début du coup ; avec 10 ms, le volume est déjà bas.
        Signal hit ((size_t) length, 0.0f);
        for (int i = 22050; i < 22050 + 2000; ++i)
            hit[(size_t) i] = 0.9f * (float) std::sin (juce::MathConstants<double>::twoPi * 100.0 * (i - 22050) / rate);
        Params p;
        p.attackMs = 8.0f;
        p.depthDb = 24.0f;
        const auto plain = render (p, hit, hit);
        p.lookaheadMs = 10.0f;
        const auto ahead = render (p, hit, hit);
        const float plainPeak = rms (plain.left, 22050, 22050 + 200);
        const float aheadPeak = rms (ahead.left, 22050 + ahead.latency, 22050 + ahead.latency + 200);
        check (ahead.latency == 441, "latence annoncée : 441 échantillons à 44,1 kHz");
        check (db (aheadPeak / plainPeak) < -10.0f, "début du coup déjà baissé (" + juce::String (db (aheadPeak / plainPeak), 1) + " dB)");
    }

    std::cout << "Robustesse" << std::endl;
    {
        bool finite = true;
        std::mt19937 gen (3);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        for (int run = 0; run < 12; ++run)
        {
            Params p;
            p.mode = (Mode) (run % 3);
            p.trigger = (Trigger) ((run / 3) % 3);
            p.depthDb = 60.0f * u (gen);
            p.attackMs = 0.1f + 300.0f * u (gen);
            p.releaseMs = 5.0f + 2000.0f * u (gen);
            p.curve = 2.0f * u (gen) - 1.0f;
            p.lookaheadMs = 20.0f * u (gen);
            p.crossoverLow = 40.0f + 900.0f * u (gen);
            p.crossoverHigh = 800.0f + 11000.0f * u (gen);
            p.focusLow = 20.0f + 5000.0f * u (gen);
            p.focusHigh = 100.0f + 19900.0f * u (gen);
            p.invert = run % 2 == 0;
            const auto r = render (p, noise (1.0f, length / 2, (unsigned) run), noise (1.0f, length / 2, (unsigned) run + 50),
                                   { { 1000, 1.0f } }, Transport { 140.0, 0.0, true });
            for (auto x : r.left) finite = finite && std::isfinite (x) && std::abs (x) < 4.0f;
        }
        check (finite, "réglages au hasard, bruit à pleine échelle : signal sain");
    }

    std::cout << (allOk ? "Tout est bon." : "Des tests ont échoué.") << std::endl;
    return allOk ? 0 : 1;
}
