// Contrôles automatiques du moteur LP-60, et rendu d'une démo en WAV.
// Utilisation : RenderTest <sortie.wav>
//               RenderTest --bench                       (charge processeur et latence)
//               RenderTest --render <scenario.txt> <sortie.wav>
//
// Un scénario décrit un rendu, une instruction par ligne :
//   rate 44100            fréquence d'échantillonnage
//   duration 9.5          durée en secondes
//   param cutoff 4        réglage de lp60::Params (même nom que le champ)
//   chorus 2              chorus : 0 coupé, 1 = I, 2 = II, 3 = I+II
//   hiss 5                souffle du chorus (0 à 10)
//   note 48 0.5 6.0       note MIDI, début et durée en secondes
// L'outil de comparaison avec le JU-06A (Tools/compare.py) s'en sert.

#include <juce_audio_formats/juce_audio_formats.h>

#include <functional>
#include <map>

#include "../Source/Arpeggiator.h"
#include "../Source/Chorus.h"
#include "../Source/LP60Engine.h"

namespace
{
    constexpr double sampleRate = 48000.0;

    bool setParam (lp60::Params& p, const juce::String& name, float v)
    {
        const std::map<juce::String, std::function<void (float)>> setters {
            { "lfoRate", [&] (float x) { p.lfoRate = x; } },     { "lfoDelay", [&] (float x) { p.lfoDelay = x; } },
            { "lfoMode", [&] (float x) { p.lfoMode = (int) x; } }, { "range", [&] (float x) { p.range = (int) x; } },
            { "dcoLfo", [&] (float x) { p.dcoLfo = x; } },       { "pwm", [&] (float x) { p.pwm = x; } },
            { "pwmMode", [&] (float x) { p.pwmMode = (int) x; } }, { "pulse", [&] (float x) { p.pulse = x > 0.5f; } },
            { "saw", [&] (float x) { p.saw = x > 0.5f; } },      { "sub", [&] (float x) { p.sub = x; } },
            { "noise", [&] (float x) { p.noise = x; } },         { "hpf", [&] (float x) { p.hpf = (int) x; } },
            { "cutoff", [&] (float x) { p.cutoff = x; } },       { "resonance", [&] (float x) { p.resonance = x; } },
            { "envInvert", [&] (float x) { p.envInvert = x > 0.5f; } }, { "vcfEnv", [&] (float x) { p.vcfEnv = x; } },
            { "vcfLfo", [&] (float x) { p.vcfLfo = x; } },       { "vcfKybd", [&] (float x) { p.vcfKybd = x; } },
            { "vcaGate", [&] (float x) { p.vcaGate = x > 0.5f; } }, { "level", [&] (float x) { p.level = x; } },
            { "attack", [&] (float x) { p.attack = x; } },       { "decay", [&] (float x) { p.decay = x; } },
            { "sustain", [&] (float x) { p.sustain = x; } },     { "release", [&] (float x) { p.release = x; } },
            { "tune", [&] (float x) { p.tune = x; } },
        };
        const auto it = setters.find (name);
        if (it == setters.end())
            return false;
        it->second (v);
        return true;
    }

    // Rend un scénario (voir l'en-tête du fichier) en WAV stéréo 24 bits.
    int renderScenario (const juce::File& scenario, const juce::File& output)
    {
        lp60::Params params;
        double rate = 44100.0, duration = 10.0;
        int chorusMode = 0;
        float hiss = 5.0f;
        struct Event { juce::int64 sample; bool on; int note; };
        std::vector<Event> events;

        juce::StringArray lines;
        scenario.readLines (lines);
        for (const auto& line : lines)
        {
            const auto t = juce::StringArray::fromTokens (line.trim(), true);
            if (t.isEmpty())
                continue;
            if (t[0] == "rate")          rate = t[1].getDoubleValue();
            else if (t[0] == "duration") duration = t[1].getDoubleValue();
            else if (t[0] == "chorus")   chorusMode = t[1].getIntValue();
            else if (t[0] == "hiss")     hiss = t[1].getFloatValue();
            else if (t[0] == "note")
            {
                const auto start = (juce::int64) std::round (t[2].getDoubleValue() * rate);
                const auto end = (juce::int64) std::round ((t[2].getDoubleValue() + t[3].getDoubleValue()) * rate);
                events.push_back ({ start, true, t[1].getIntValue() });
                events.push_back ({ end, false, t[1].getIntValue() });
            }
            else if (t[0] == "param" && setParam (params, t[1], t[2].getFloatValue())) {}
            else
            {
                std::cerr << "ligne inconnue : " << line << std::endl;
                return 2;
            }
        }
        std::sort (events.begin(), events.end(), [] (const Event& a, const Event& b)
                   { return a.sample != b.sample ? a.sample < b.sample : ! a.on && b.on; });

        lp60::Engine engine;
        engine.setParams (params);
        engine.prepare (rate, 256);

        const auto total = (int) std::round (duration * rate);
        juce::AudioBuffer<float> audio (2, total);
        audio.clear();
        auto* left = audio.getWritePointer (0);

        size_t next = 0;
        for (int pos = 0; pos < total;)
        {
            while (next < events.size() && events[next].sample <= pos)
            {
                events[next].on ? engine.noteOn (events[next].note) : engine.noteOff (events[next].note);
                ++next;
            }
            int end = juce::jmin (total, pos + 256);
            if (next < events.size())
                end = (int) juce::jmin ((juce::int64) end, juce::jmax ((juce::int64) pos + 1, events[next].sample));
            engine.render (left + pos, end - pos);
            pos = end;
        }

        if (chorusMode > 0)
        {
            lp60::Chorus chorus;
            chorus.prepare (rate);
            chorus.setMode (chorusMode);
            chorus.setHiss (hiss);
            chorus.process (left, audio.getWritePointer (1), total);
        }
        else
        {
            audio.copyFrom (1, 0, audio, 0, 0, total);
        }

        output.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (output.createOutputStream().release());
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), rate, 2, 24, {}, 0));
        if (writer == nullptr)
            return 1;
        stream.release();
        writer->writeFromAudioSampleBuffer (audio, 0, total);
        return 0;
    }

    bool check (bool condition, const juce::String& what)
    {
        std::cout << (condition ? "  OK    " : "  ECHEC ") << what << std::endl;
        return condition;
    }

    struct Render
    {
        std::vector<float> samples;
        bool finite = true;
        float peak = 0.0f;

        float rms (double from, double to) const
        {
            const auto a = (size_t) (from * sampleRate), b = juce::jmin (samples.size(), (size_t) (to * sampleRate));
            double sum = 0.0;
            for (size_t i = a; i < b; ++i)
                sum += (double) samples[i] * samples[i];
            return b > a ? (float) std::sqrt (sum / (double) (b - a)) : 0.0f;
        }
    };

    // Joue des notes pendant holdSeconds puis les relâche ; rend totalSeconds.
    Render play (const lp60::Params& params, std::initializer_list<int> notes, double holdSeconds, double totalSeconds,
                 int* voicesWhileHeld = nullptr)
    {
        lp60::Engine engine;
        engine.setParams (params);
        engine.prepare (sampleRate, 256);
        for (int n : notes)
            engine.noteOn (n);

        Render r;
        r.samples.resize ((size_t) (totalSeconds * sampleRate));
        bool released = false;
        for (size_t pos = 0; pos < r.samples.size(); pos += 256)
        {
            if (! released && (double) pos / sampleRate >= holdSeconds)
            {
                if (voicesWhileHeld != nullptr)
                    *voicesWhileHeld = engine.activeVoices();
                for (int n : notes)
                    engine.noteOff (n);
                released = true;
            }
            const int count = (int) juce::jmin ((size_t) 256, r.samples.size() - pos);
            engine.render (r.samples.data() + pos, count);
        }
        for (float x : r.samples)
        {
            r.finite = r.finite && std::isfinite (x);
            r.peak = std::max (r.peak, std::abs (x));
        }
        return r;
    }

    // Durée d'une phase d'enveloppe, mesurée sur l'enveloppe seule (secondes).
    double attackDuration (float slider)
    {
        const float rate = 96000.0f;
        lp60::detail::Envelope env;
        env.gateOn();
        const float coef = lp60::detail::rcCoef (lp60::detail::lookup (lp60::detail::attackTimes, slider) / std::log (2.0f), rate);
        int n = 0;
        while (env.stage == lp60::detail::Envelope::Stage::attack && n < (int) (rate * 30))
        {
            env.process (coef, 0.0f, 1.0f, 0.0f);
            ++n;
        }
        return n / (double) rate;
    }

    double decayDuration (float slider)   // jusqu'à -30 dB du niveau de départ, sustain à 0
    {
        const float rate = 96000.0f;
        lp60::detail::Envelope env;
        env.stage = lp60::detail::Envelope::Stage::decay;
        env.level = 1.0f;
        const float coef = lp60::detail::rcCoef (lp60::detail::lookup (lp60::detail::envTimeConstants, slider), rate);
        int n = 0;
        while (env.level > std::pow (10.0f, -30.0f / 20.0f) && n < (int) (rate * 60))
        {
            env.process (0.0f, coef, 0.0f, 0.0f);
            ++n;
        }
        return n / (double) rate;
    }

    float selfOscillationHz (float cutoff)
    {
        const float rate = 96000.0f;
        lp60::detail::OtaFilter filter;
        const float g = std::tan (juce::MathConstants<float>::pi * cutoff / rate);
        juce::Random random (1);
        int crossings = 0;
        float previous = 0.0f;
        for (int i = 0; i < (int) rate * 2; ++i)
        {
            const float y = filter.process ((random.nextFloat() - 0.5f) * 1.0e-5f, g, 4.4f);
            if (i >= (int) rate && previous <= 0.0f && y > 0.0f)
                ++crossings;
            previous = y;
        }
        return (float) crossings;
    }

    std::vector<int> arpNotes (const lp60::ArpSettings& settings)
    {
        lp60::Arpeggiator arp;
        arp.prepare (sampleRate);
        std::vector<int> notes;
        for (int block = 0; block < 2 * 48000 / 512; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                for (int n : { 64, 60, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
            arp.process (midi, 512, settings, 120.0, false, 0.0);
            for (const auto m : midi)
                if (m.getMessage().isNoteOn())
                    notes.push_back (m.getMessage().getNoteNumber());
        }
        return notes;
    }

    float chorusWidth (int mode)
    {
        lp60::Chorus chorus;
        chorus.prepare (sampleRate);
        chorus.setMode (mode);
        std::vector<float> l (96000), r (96000);
        for (size_t i = 0; i < l.size(); ++i)
            l[i] = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * (float) i / 48000.0f);
        chorus.process (l.data(), r.data(), (int) l.size());
        double diff = 0.0;
        for (size_t i = 48000; i < l.size(); ++i)
            diff += std::abs (l[i] - r[i]);
        return (float) (diff / 48000.0);
    }
}

int main (int argc, char* argv[])
{
    if (argc >= 4 && juce::String (argv[1]) == "--render")
    {
        const auto cwd = juce::File::getCurrentWorkingDirectory();
        return renderScenario (cwd.getChildFile (argv[2]), cwd.getChildFile (argv[3]));
    }

    if (argc >= 2 && juce::String (argv[1]) == "--bench")
    {
        lp60::Params p;
        p.pulse = true;
        p.sub = 5.0f;
        lp60::Engine engine;
        engine.setParams (p);
        engine.prepare (sampleRate, 256);
        for (int n : { 48, 52, 55, 60, 64, 67 })
            engine.noteOn (n);
        std::vector<float> block (256);
        const int blocks = 10 * 48000 / 256;
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < blocks; ++i)
            engine.render (block.data(), 256);
        const double seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        std::cout << "Latence : " << engine.getLatencySamples() << " echantillons" << std::endl;
        std::cout << "6 voix : " << juce::String (100.0 * seconds / 10.0, 2) << " % d'un coeur" << std::endl;
        return 0;
    }

    if (argc < 2)
    {
        std::cerr << "usage: RenderTest <sortie.wav> | --bench" << std::endl;
        return 2;
    }

    bool ok = true;

    // Polyphonie : 6 voix, la septième note en vole une.
    {
        lp60::Params p;
        int voices = 0;
        auto chord = play (p, { 48, 52, 55, 60, 64, 67, 72 }, 1.0, 2.0, &voices);
        ok &= check (chord.finite, "aucun NaN / infini");
        ok &= check (voices == 6, "7 notes jouees : 6 voix actives, comme le Juno-60 (" + juce::String (voices) + ")");
        ok &= check (chord.rms (0.2, 0.9) > 0.02f && chord.peak < 1.0f,
                     "l'accord produit du son, sans saturer la sortie (crete " + juce::String (chord.peak, 2) + ")");
        ok &= check (chord.rms (1.9, 2.0) < 1.0e-3f, "silence apres le relachement");
    }

    // Passe-haut : la position 3 (720 Hz) retire les graves d'un do grave.
    {
        lp60::Params p;
        p.cutoff = 10.0f;
        auto open = play (p, { 36 }, 1.0, 1.0);
        p.hpf = 3;
        auto thin = play (p, { 36 }, 1.0, 1.0);
        ok &= check (thin.rms (0.2, 1.0) < open.rms (0.2, 1.0) * 0.6f, "HPF position 3 : graves retires");
    }

    // Enveloppe : durées mesurées sur le JU-06A (tests C du kit).
    {
        const double a10 = attackDuration (10.0f), a5 = attackDuration (5.0f), d10 = decayDuration (10.0f);
        ok &= check (std::abs (a10 - 3.0) < 0.1, "attaque au maximum : 3,0 s (" + juce::String (a10, 2) + " s)");
        ok &= check (std::abs (a5 - 0.15) < 0.02, "attaque a mi-course : 0,15 s (" + juce::String (a5, 3) + " s)");
        ok &= check (std::abs (d10 - 30.0 / 2.28) < 0.6, "decay au maximum : 2,28 dB/s, -30 dB en 13 s (" + juce::String (d10, 1) + " s)");
    }

    // Mode GATE : le son s'arrête en quelques millisecondes.
    {
        lp60::Params p;
        p.vcaGate = true;
        p.release = 10.0f;
        auto gate = play (p, { 60 }, 0.5, 0.6);
        ok &= check (gate.rms (0.52, 0.6) < 1.0e-3f, "VCA GATE : coupure nette au relachement malgre un long RELEASE");
    }

    // Filtre : auto-oscillation accordée sur la coupure.
    for (float cutoff : { 248.0f, 992.0f })
    {
        const float hz = selfOscillationHz (cutoff);
        ok &= check (std::abs (hz / cutoff - 1.0f) < 0.05f,
                     "VCF auto-oscille a " + juce::String (hz, 0) + " Hz pour une coupure de " + juce::String (cutoff, 0) + " Hz");
    }

    // Arpège UP sur 2 octaves, et DOWN.
    {
        lp60::ArpSettings arp;
        arp.on = true;
        arp.range = 2;
        auto notes = arpNotes (arp);
        ok &= check (notes.size() >= 7 && notes[0] == 60 && notes[3] == 72 && notes[5] == 79 && notes[6] == 60,
                     "arpege UP sur 2 octaves : do mi sol, une octave au-dessus, puis retour");
        arp.mode = 2;
        arp.range = 1;
        notes = arpNotes (arp);
        ok &= check (notes.size() >= 3 && notes[0] == 67 && notes[1] == 64 && notes[2] == 60, "arpege DOWN : sol, mi, do");
    }

    // Chorus : I et II stéréo, I+II mono.
    ok &= check (chorusWidth (1) > 0.01f && chorusWidth (2) > 0.01f, "chorus I et II : stereo");
    ok &= check (chorusWidth (3) > 0.01f, "chorus I+II : stereo aussi (mesure sur le JU-06A)");

    // Souffle du chorus : présent sans note, au niveau mesuré sur le JU-06A, absent à 0.
    {
        auto hissRms = [] (float amount)
        {
            lp60::Chorus chorus;
            chorus.prepare (sampleRate);
            chorus.setMode (1);
            chorus.setHiss (amount);
            std::vector<float> l (96000, 0.0f), r (96000, 0.0f);
            chorus.process (l.data(), r.data(), (int) l.size());
            double sum = 0.0;
            for (size_t i = 48000; i < l.size(); ++i)
                sum += (double) l[i] * l[i];
            return (float) std::sqrt (sum / 48000.0);
        };
        const float db = juce::Decibels::gainToDecibels (hissRms (5.0f));
        ok &= check (std::abs (db + 76.2f) < 1.0f, "souffle du chorus a -76 dBFS par canal, comme le JU-06A (" + juce::String (db, 1) + " dB)");
        ok &= check (hissRms (0.0f) == 0.0f, "souffle a 0 : silence total");
    }

    // Tout à fond : rien ne s'emballe.
    {
        lp60::Params p;
        p.pulse = true;
        p.sub = p.noise = p.resonance = p.vcfEnv = p.vcfLfo = p.dcoLfo = p.pwm = p.level = 10.0f;
        p.cutoff = 10.0f;
        p.pwmMode = 0;
        auto wild = play (p, { 36, 48, 60, 72, 84, 96 }, 1.5, 2.0);
        ok &= check (wild.finite && wild.peak < 4.0f, "tout a fond, 6 voix : signal sain (crete " + juce::String (wild.peak, 2) + ")");
    }

    // Démo : un accord de pad avec PWM et chorus II.
    {
        lp60::Params p;
        p.pulse = true;
        p.pwm = 5.0f;
        p.pwmMode = 0;
        p.lfoRate = 3.0f;
        p.cutoff = 5.5f;
        p.vcfEnv = 2.0f;
        p.attack = 6.0f;
        p.release = 6.0f;
        auto demo = play (p, { 48, 55, 60, 64 }, 3.0, 6.0);

        lp60::Chorus chorus;
        chorus.prepare (sampleRate);
        chorus.setMode (2);
        juce::AudioBuffer<float> stereo (2, (int) demo.samples.size());
        stereo.copyFrom (0, 0, demo.samples.data(), stereo.getNumSamples());
        chorus.process (stereo.getWritePointer (0), stereo.getWritePointer (1), stereo.getNumSamples());

        juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0));
        if (writer != nullptr)
        {
            stream.release();
            writer->writeFromAudioSampleBuffer (stereo, 0, stereo.getNumSamples());
            std::cout << "WAV ecrit : " << file.getFullPathName() << std::endl;
        }
    }

    return ok ? 0 : 1;
}
