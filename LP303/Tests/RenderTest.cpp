// Contrôles automatiques de LP-303 (filtre, slide, accent, séquenceur) et rendu
// d'une démo en WAV.
// Utilisation : RenderTest [demo.wav]
//               RenderTest --bench        (charge processeur)

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <functional>
#include <iostream>
#include <vector>

#include "../Source/AcidEngine.h"
#include "../Source/Presets.h"
#include "../Source/Sequencer.h"

namespace
{
    bool allOk = true;

    void check (bool condition, const juce::String& what)
    {
        std::cout << (condition ? "  OK    " : "  ECHEC ") << what << std::endl;
        allOk = allOk && condition;
    }

    struct Event
    {
        int sample;
        juce::MidiMessage message;
    };

    // Rend le moteur par blocs de 256 avec des événements MIDI datés ; appelle
    // `probe` après chaque bloc (pour suivre l'état interne).
    std::vector<float> render (lp303::AcidEngine& engine, std::vector<Event> events, int numSamples,
                               const std::function<void (int)>& probe = nullptr)
    {
        std::vector<float> out ((size_t) numSamples, 0.0f);
        std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.sample < b.sample; });

        size_t next = 0;
        constexpr int block = 256;
        for (int start = 0; start < numSamples; start += block)
        {
            const int end = juce::jmin (numSamples, start + block);
            int pos = start;
            while (next < events.size() && events[next].sample < end)
            {
                const int t = juce::jmax (pos, events[next].sample);
                if (t > pos)
                    engine.render (out.data() + pos, t - pos);
                pos = t;

                const auto& m = events[next].message;
                if (m.isNoteOn())
                    engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
                else if (m.isNoteOff())
                    engine.noteOff (m.getNoteNumber());
                ++next;
            }
            if (end > pos)
                engine.render (out.data() + pos, end - pos);
            if (probe)
                probe (end);
        }
        return out;
    }

    float rms (const std::vector<float>& x, int from, int to)
    {
        double sum = 0.0;
        for (int i = from; i < to; ++i)
            sum += (double) x[(size_t) i] * x[(size_t) i];
        return (float) std::sqrt (sum / juce::jmax (1, to - from));
    }

    float peak (const std::vector<float>& x)
    {
        float p = 0.0f;
        for (auto v : x)
            p = juce::jmax (p, std::abs (v));
        return p;
    }

    bool finite (const std::vector<float>& x)
    {
        for (auto v : x)
            if (! std::isfinite (v))
                return false;
        return true;
    }

    // Brillance : énergie de la dérivée rapportée à celle du signal.
    float brightness (const std::vector<float>& x, int from, int to)
    {
        double d = 0.0, s = 0.0;
        for (int i = from + 1; i < to; ++i)
        {
            const double diff = (double) x[(size_t) i] - x[(size_t) i - 1];
            d += diff * diff;
            s += (double) x[(size_t) i] * x[(size_t) i];
        }
        return s > 0.0 ? (float) std::sqrt (d / s) : 0.0f;
    }

    juce::MidiMessage on (int note, int velocity) { return juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity); }
    juce::MidiMessage off (int note)              { return juce::MidiMessage::noteOff (1, note); }

    //==========================================================================
    void testFilterStability()
    {
        std::cout << "Filtre a diodes, resonance maximale" << std::endl;

        // Le filtre seul, au-delà du gain critique, avec une entrée énorme puis rien.
        for (float fc : { 30.0f, 300.0f, 3000.0f, 18000.0f })
        {
            lp303::DiodeLadder ladder;
            const float g = std::tan (juce::MathConstants<float>::pi * fc / 176400.0f);
            float biggest = 0.0f;
            bool ok = true;
            for (int i = 0; i < 176400 * 2; ++i)
            {
                const float in = i < 176400 ? ((i / 200) % 2 == 0 ? 8.0f : -8.0f) : 0.0f;
                const float y = ladder.process (in, g, lp303::DiodeLadder::criticalK * 1.1f);
                ok = ok && std::isfinite (y);
                biggest = juce::jmax (biggest, std::abs (y));
            }
            check (ok && biggest < 5.0f, "filtre seul, k au-dela du critique, fc " + juce::String (fc) + " Hz : borne (crete "
                                             + juce::String (biggest, 2) + ")");
        }

        // Le moteur complet : resonance, env mod et accent au maximum, toutes les
        // notes, coupure balayée, avec et sans saturation, à trois fréquences.
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            for (float drive : { 0.0f, 10.0f })
            {
                lp303::AcidEngine engine;
                engine.prepare (rate, 512);
                lp303::Params p;
                p.resonance = 10.0f;
                p.envMod = 10.0f;
                p.accent = 10.0f;
                p.decay = 10.0f;
                p.volume = 10.0f;
                p.drive = drive;

                bool ok = true;
                float biggest = 0.0f;
                const int noteLength = (int) (rate * 0.12);
                for (int wave = 0; wave < 2; ++wave)
                {
                    p.wave = wave;
                    for (int i = 0; i <= 20; ++i)
                    {
                        p.cutoff = (float) i * 0.5f;
                        engine.setParams (p);
                        const int note = 24 + (i * 7) % 61;
                        auto out = render (engine, { { 0, on (note, (i % 2) != 0 ? 127 : 90) }, { noteLength - 100, off (note) } },
                                           noteLength);
                        ok = ok && finite (out);
                        biggest = juce::jmax (biggest, peak (out));
                    }
                }
                check (ok && biggest < 3.0f, "moteur a " + juce::String ((int) rate) + " Hz, drive " + juce::String (drive, 0)
                                                 + " : pas de NaN, crete " + juce::String (biggest, 2));
            }
        }
    }

    // Repliement et sons parasites : note tenue, résonance au maximum. Une fois
    // le tintement de l'attaque éteint, le spectre ne doit contenir que les
    // harmoniques de la note : un pic ailleurs serait du repliement (aliasing)
    // ou une sous-harmonique due à la saturation du filtre.
    double worstInharmonicPeak (int note, int wave, float cutoff, bool accent)
    {
        constexpr double rate = 44100.0;
        constexpr int fftOrder = 14, fftSize = 1 << fftOrder, skip = 44100;

        lp303::AcidEngine engine;
        engine.prepare (rate, 512);
        lp303::Params p;
        p.wave = wave;
        p.cutoff = cutoff;
        p.resonance = 10.0f;
        p.envMod = 0.0f;
        p.accent = 10.0f;
        p.volume = 10.0f;
        engine.setParams (p);

        auto out = render (engine, { { 0, on (note, accent ? 127 : 90) } }, fftSize + skip);
        std::vector<float> data ((size_t) fftSize * 2, 0.0f);
        for (int i = 0; i < fftSize; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);
            data[(size_t) i] = out[(size_t) (i + skip)] * w;
        }
        juce::dsp::FFT fft (fftOrder);
        fft.performFrequencyOnlyForwardTransform (data.data());

        const double f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        const double binHz = rate / fftSize;
        double harmonic = 1.0e-30, other = 1.0e-30;
        for (int b = 3; b < fftSize / 2 - 1; ++b)
        {
            const double h = b * binHz / f0;
            const double distance = std::abs (h - std::round (h)) * f0 / binHz;
            const double e = (double) data[(size_t) b] * data[(size_t) b];
            if (distance <= 4.0 && std::round (h) >= 1.0)
                harmonic = juce::jmax (harmonic, e);
            else if (b * binHz > 20.0 && data[(size_t) b] > data[(size_t) b - 1] && data[(size_t) b] >= data[(size_t) b + 1])
                other = juce::jmax (other, e);
        }
        return 10.0 * std::log10 (other / harmonic);
    }

    void testAliasing()
    {
        std::cout << "Repliement et sons parasites, resonance maximale" << std::endl;

        for (int wave = 0; wave < 2; ++wave)
        {
            double worst = -300.0;
            juce::String where;
            for (int note : { 33, 45, 57, 69, 81, 93 })
                for (float cutoff : { 1.0f, 3.0f, 5.0f, 7.5f, 10.0f })
                    for (bool accent : { false, true })
                        if (60.0 * std::pow (2.0, 0.7 * cutoff) >= 0.5 * 440.0 * std::pow (2.0, (note - 69) / 12.0))   // note dans la bande passante
                            if (const double w = worstInharmonicPeak (note, wave, cutoff, accent); w > worst)
                            {
                                worst = w;
                                where = "note " + juce::String (note) + ", cutoff " + juce::String (cutoff, 1) + (accent ? ", accent" : "");
                            }

            check (worst < -50.0, juce::String (wave == 0 ? "dents de scie" : "carre") + " : pic parasite le plus fort "
                                      + juce::String (worst, 1) + " dB (" + where + ")");
        }
    }

    //==========================================================================
    void testSlide()
    {
        std::cout << "Slide et legato" << std::endl;

        constexpr double rate = 48000.0;
        lp303::AcidEngine engine;
        engine.prepare (rate, 512);
        lp303::Params p;
        p.slide = 0.08f;
        engine.setParams (p);

        const int second = (int) (0.3 * rate);
        float pitchSoonAfter = 0.0f, pitchLater = 0.0f, envBefore = 0.0f, envAfter = 0.0f;
        bool soonDone = false, laterDone = false;
        render (engine, { { 0, on (36, 90) }, { second, on (48, 90) }, { second + 10, off (36) } }, (int) (0.8 * rate),
                [&] (int pos)
                {
                    if (pos == second - (second % 256))                 envBefore = engine.getFilterEnvelope();
                    if (pos >= second + 480 && ! soonDone)  { soonDone = true; pitchSoonAfter = engine.getPitch(); envAfter = engine.getFilterEnvelope(); }
                    if (pos >= second + (int) (0.4 * rate) && ! laterDone) { laterDone = true; pitchLater = engine.getPitch(); }
                });

        check (pitchSoonAfter > 36.5f && pitchSoonAfter < 47.5f, "note liee : la hauteur glisse (10 ms apres : "
                                                                      + juce::String (pitchSoonAfter, 2) + ")");
        check (std::abs (pitchLater - 48.0f) < 0.05f, "le glissement arrive sur la note (" + juce::String (pitchLater, 3) + ")");
        check (envAfter <= envBefore + 1.0e-6f && envAfter < 0.9f, "note liee : l'enveloppe du filtre ne repart pas");

        // Notes détachées : la hauteur saute et l'enveloppe repart.
        lp303::AcidEngine detached;
        detached.prepare (rate, 512);
        detached.setParams (p);
        float pitchJump = 0.0f, envJump = 0.0f;
        bool jumpDone = false;
        render (detached, { { 0, on (36, 90) }, { second - 2000, off (36) }, { second, on (48, 90) } }, second + 512,
                [&] (int pos)
                {
                    if (pos > second && ! jumpDone) { jumpDone = true; pitchJump = detached.getPitch(); envJump = detached.getFilterEnvelope(); }
                });
        check (std::abs (pitchJump - 48.0f) < 1.0e-3f && envJump > 0.9f, "note detachee : la hauteur saute, l'enveloppe repart");
    }

    void testAccent()
    {
        std::cout << "Accent" << std::endl;

        constexpr double rate = 48000.0;

        // Fréquence redéclarée dans la lambda : MSVC refuse d'y lire une
        // constante de la fonction sans la capturer.
        auto play = [] (int velocity, float& maxCutoff, float& envAt150)
        {
            constexpr double noteRate = 48000.0;
            lp303::AcidEngine engine;
            engine.prepare (noteRate, 512);
            lp303::Params p;
            p.cutoff = 3.0f;
            p.resonance = 6.0f;
            p.envMod = 5.0f;
            p.decay = 4.0f;
            p.accent = 8.0f;
            engine.setParams (p);
            maxCutoff = 0.0f;
            envAt150 = -1.0f;
            return render (engine, { { 0, on (36, velocity) } }, (int) (0.3 * noteRate),
                           [&] (int pos)
                           {
                               maxCutoff = juce::jmax (maxCutoff, engine.getCutoffHz());
                               if (pos >= (int) (0.15 * noteRate) && envAt150 < 0.0f)
                                   envAt150 = engine.getFilterEnvelope();
                           });
        };

        float cutNormal, cutAccent, envNormal, envAccent, cut99, env99;
        const auto normal = play (90, cutNormal, envNormal);
        const auto accent = play (127, cutAccent, envAccent);
        const auto at99 = play (99, cut99, env99);
        float cut100, env100;
        play (100, cut100, env100);

        const int window = (int) (0.1 * rate);
        const float gainDb = juce::Decibels::gainToDecibels (rms (accent, 0, window) / rms (normal, 0, window));
        check (gainDb > 3.0f, "accent plus fort : +" + juce::String (gainDb, 1) + " dB");
        check (cutAccent > cutNormal * 1.5f, "accent : la coupure monte plus haut (" + juce::String (cutNormal, 0) + " -> "
                                                 + juce::String (cutAccent, 0) + " Hz)");
        check (brightness (accent, 0, window) > brightness (normal, 0, window), "accent plus mordant (plus brillant)");
        check (envAccent < envNormal * 0.5f, "accent : decroissance du filtre la plus courte");
        check (std::abs (env99 - envNormal) < 1.0e-6f && env100 < envNormal * 0.5f, "velocite 99 = normale, 100 = accent");
        juce::ignoreUnused (at99, cut99);
    }

    //==========================================================================
    struct SeqEvent
    {
        juce::int64 sample;
        bool isOn;
        int note, velocity;
    };

    // Fait tourner le séquenceur par blocs de tailles variables, sur un
    // transport qui avance normalement depuis startPpq.
    std::vector<SeqEvent> runSequencer (lp303::Sequencer& seq, const lp303::PatternBank& bank, lp303::SequencerSettings settings,
                                        double bpm, double startPpq, juce::int64 numSamples, double rate = 48000.0,
                                        const std::function<void (juce::int64, double&, lp303::SequencerSettings&)>& hook = nullptr)
    {
        std::vector<SeqEvent> events;
        juce::Random random (42);
        juce::int64 pos = 0;
        double ppq = startPpq;
        while (pos < numSamples)
        {
            const int block = juce::jmin ((int) (numSamples - pos), 32 + random.nextInt (700));
            if (hook)
                hook (pos, ppq, settings);
            juce::MidiBuffer midi;
            seq.process (midi, block, bank, settings, bpm, ppq);
            for (const auto m : midi)
            {
                const auto msg = m.getMessage();
                if (msg.isNoteOnOrOff())
                    events.push_back ({ pos + m.samplePosition, msg.isNoteOn(), msg.getNoteNumber(), msg.getVelocity() });
            }
            pos += block;
            ppq += block * bpm / 60.0 / rate;
        }
        return events;
    }

    std::vector<SeqEvent> onsOnly (const std::vector<SeqEvent>& e)
    {
        std::vector<SeqEvent> r;
        for (const auto& x : e)
            if (x.isOn)
                r.push_back (x);
        return r;
    }

    void testSequencer()
    {
        std::cout << "Sequenceur" << std::endl;
        constexpr double rate = 48000.0;

        // Motif de test : 16 notes différentes, toutes jouées.
        lp303::PatternBank bank;
        for (int s = 0; s < lp303::numSteps; ++s)
        {
            lp303::Step st;
            st.note = s % 13;
            st.octave = s >= 13 ? 1 : 0;
            bank.set (0, s, st);
        }

        lp303::SequencerSettings settings;
        settings.running = true;

        // 120 BPM à 48 kHz : une double croche = 6000 échantillons.
        {
            lp303::Sequencer seq;
            seq.prepare (rate);
            const auto events = runSequencer (seq, bank, settings, 120.0, 0.0, 6000 * 32);
            const auto ons = onsOnly (events);
            bool onTime = ons.size() == 32, offTime = true, notes = true;
            for (size_t i = 0; i < ons.size() && i < 32; ++i)
            {
                onTime = onTime && ons[i].sample == (juce::int64) i * 6000;
                notes = notes && ons[i].note == bank.get (0, (int) i % 16).midiNote (0);
            }
            for (const auto& e : events)
                if (! e.isOn)
                    offTime = offTime && (e.sample % 6000 == 3000);
            check (onTime, "120 BPM : chaque pas tombe a l'echantillon pres (32 pas)");
            check (offTime, "les notes durent une demi double croche");
            check (notes, "les notes suivent le motif et bouclent sur 16 pas");
        }

        // Accent, slide, silence.
        {
            lp303::PatternBank b2;
            lp303::Step a;  a.note = 0; a.accent = true;
            lp303::Step sl; sl.note = 3; sl.slide = true;
            lp303::Step t;  t.note = 7;
            lp303::Step rest; rest.gate = false;
            b2.set (0, 0, a);
            b2.set (0, 1, sl);
            b2.set (0, 2, t);
            b2.set (0, 3, rest);
            b2.setLength (0, 4);

            lp303::Sequencer seq;
            seq.prepare (rate);
            const auto events = runSequencer (seq, b2, settings, 120.0, 0.0, 6000 * 4);

            const bool accentOk = ! events.empty() && events[0].isOn && events[0].velocity == 127;
            bool slideOk = false, restOk = true, normalVel = true;
            for (size_t i = 0; i + 1 < events.size(); ++i)
                if (events[i].isOn && events[i].note == 43 && events[i].sample == 12000)
                    slideOk = ! events[i + 1].isOn && events[i + 1].note == 39 && events[i + 1].sample == 12000;
            for (const auto& e : events)
            {
                if (e.isOn && e.sample >= 18000) restOk = false;
                if (e.isOn && e.note != 36 && e.velocity != 90) normalVel = false;
                if (! e.isOn && e.note == 39 && e.sample < 12000) slideOk = false;   // pas de coupure avant le pas suivant
            }
            check (accentOk && normalVel, "accent : velocite 127, sinon 90");
            check (slideOk, "slide : la note suivante arrive avant la fin de la precedente (legato)");
            check (restOk, "silence : aucune note");
        }

        // Longueur de motif.
        {
            bank.setLength (0, 7);
            lp303::Sequencer seq;
            seq.prepare (rate);
            const auto ons = onsOnly (runSequencer (seq, bank, settings, 120.0, 0.0, 6000 * 21));
            bool ok = ons.size() == 21;
            for (size_t i = 0; i < ons.size(); ++i)
                ok = ok && ons[i].note == bank.get (0, (int) (i % 7)).midiNote (0);
            check (ok, "longueur 7 : le motif boucle tous les 7 pas");
            bank.setLength (0, 16);
        }

        // Démarrage au milieu d'une mesure, tempo 140, swing.
        {
            lp303::Sequencer seq;
            seq.prepare (rate);
            const auto ons = onsOnly (runSequencer (seq, bank, settings, 140.0, 1.1, 48000));
            const double samplesPerStep = 60.0 / 140.0 / 4.0 * rate;
            const auto first = (juce::int64) std::ceil ((1.25 - 1.1) * 4.0 * samplesPerStep - 1.0e-6);
            check (! ons.empty() && ons[0].sample == first && ons[0].note == bank.get (0, 5).midiNote (0),
                   "depart a 1,1 noire, 140 BPM : premier pas = pas 6, a l'heure");

            lp303::Sequencer swingSeq;
            swingSeq.prepare (rate);
            auto swung = settings;
            swung.swing = 1.0f;
            const auto ons2 = onsOnly (runSequencer (swingSeq, bank, swung, 120.0, 0.0, 6000 * 8));
            bool ok = ons2.size() == 8;
            for (size_t i = 0; i < ons2.size(); ++i)
                ok = ok && ons2[i].sample == (juce::int64) i * 6000 + ((i % 2) != 0 ? 3000 : 0);
            check (ok, "swing maximal : les pas impairs sont retardes d'une demi double croche");
        }

        // Boucle de l'hôte : la tête de lecture revient en arrière.
        {
            lp303::Sequencer seq;
            seq.prepare (rate);
            bool jumpedBack = false;
            const juce::int64 jumpAt = 6000 * 6 + 1234;
            const auto events = runSequencer (seq, bank, settings, 120.0, 0.0, 6000 * 10, rate,
                                              [&] (juce::int64 pos, double& ppq, lp303::SequencerSettings&)
                                              {
                                                  if (! jumpedBack && pos >= jumpAt)
                                                  {
                                                      jumpedBack = true;
                                                      ppq = 0.0;
                                                  }
                                              });
            juce::int64 jumpSample = -1;
            int onsAfterJump = 0;
            bool firstAfterJumpIsStep1 = false;
            for (const auto& e : events)
            {
                if (e.sample >= jumpAt && jumpSample < 0)
                    jumpSample = e.sample;
                if (e.isOn && jumpSample >= 0 && e.sample >= jumpSample)
                {
                    if (onsAfterJump++ == 0)
                        firstAfterJumpIsStep1 = e.note == bank.get (0, 0).midiNote (0);
                }
            }
            int sounding = 0;
            for (const auto& e : events)
                sounding += e.isOn ? 1 : -1;
            check (jumpedBack && firstAfterJumpIsStep1, "boucle de l'hote : le motif repart du pas 1");
            check (sounding <= 1, "aucune note bloquee apres le saut");
        }

        // Changement de motif : attend la fin du motif en cours.
        {
            lp303::PatternBank b3;
            for (int s = 0; s < 16; ++s)
            {
                lp303::Step x; x.note = 0;
                lp303::Step y; y.note = 12;
                b3.set (0, s, x);
                b3.set (1, s, y);
            }
            lp303::Sequencer seq;
            seq.prepare (rate);
            const auto ons = onsOnly (runSequencer (seq, b3, settings, 120.0, 0.0, 6000 * 32, rate,
                                                    [] (juce::int64 pos, double&, lp303::SequencerSettings& s)
                                                    {
                                                        if (pos >= 6000 * 5) s.pattern = 1;
                                                    }));
            bool ok = ons.size() == 32;
            for (size_t i = 0; i < ons.size(); ++i)
                ok = ok && ons[i].note == (i < 16 ? 36 : 48);
            check (ok, "changement de motif au pas 6 : le nouveau demarre au pas 17");
        }

        // Arrêt du transport : la note se coupe.
        {
            lp303::Sequencer seq;
            seq.prepare (rate);
            lp303::PatternBank b4;
            lp303::Step held; held.slide = true;
            for (int s = 0; s < 16; ++s)
                b4.set (0, s, held);
            auto events = runSequencer (seq, b4, settings, 120.0, 0.0, 9000);
            juce::MidiBuffer stopMidi;
            auto stopped = settings;
            stopped.running = false;
            seq.process (stopMidi, 256, b4, stopped, 120.0, 0.0);
            bool noteOff = false;
            for (const auto m : stopMidi)
                noteOff = noteOff || m.getMessage().isNoteOff();
            check (noteOff, "arret du transport : la note tenue (slide) se coupe");
        }
    }

    void testPatterns()
    {
        std::cout << "Motifs" << std::endl;

        lp303::PatternBank bank;
        lp303::loadFactoryPatterns (bank);
        const auto s1 = bank.get (0, 1);
        const auto s5 = bank.get (0, 5);
        const auto s4 = bank.get (0, 4);
        check (s1.accent && s1.note == 0 && s5.slide && s5.note == 3 && ! s4.gate && bank.getLength (3) == 12,
               "motifs d'usine lus (accent, slide, silence, longueur)");

        lp303::PatternBank copy;
        bool same = true;
        for (int p = 0; p < lp303::numPatterns; ++p)
        {
            same = same && copy.fromString (p, bank.toString (p));
            for (int s = 0; s < lp303::numSteps; ++s)
                same = same && copy.get (p, s) == bank.get (p, s);
            same = same && copy.getLength (p) == bank.getLength (p);
        }
        check (same, "sauvegarde et relecture des 8 motifs a l'identique");
        check (! copy.fromString (0, "n'importe quoi"), "texte illisible refuse");
    }

    //==========================================================================
    // Démo : 8 mesures du motif « Acid Roll » à 128 BPM, coupure qui s'ouvre.
    int renderDemo (const juce::File& file)
    {
        constexpr double rate = 44100.0, bpm = 128.0;
        constexpr int block = 256;
        const int numSamples = (int) (8 * 4 * 60.0 / bpm * rate) + (int) rate;

        lp303::PatternBank bank;
        lp303::loadFactoryPatterns (bank);
        lp303::Sequencer seq;
        seq.prepare (rate);
        lp303::AcidEngine engine;
        engine.prepare (rate, block);

        lp303::Params p;
        p.cutoff = 2.0f;
        p.resonance = 8.0f;
        p.envMod = 6.0f;
        p.decay = 4.0f;
        p.accent = 7.0f;
        p.drive = 2.0f;

        lp303::SequencerSettings settings;
        settings.running = true;

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();
        double ppq = 0.0;
        for (int start = 0; start < numSamples; start += block)
        {
            const int n = juce::jmin (block, numSamples - start);
            const float progress = (float) start / (float) numSamples;
            p.cutoff = 1.5f + 4.0f * std::sin (juce::MathConstants<float>::pi * progress);
            settings.running = start < numSamples - (int) rate;
            engine.setParams (p);

            juce::MidiBuffer midi;
            seq.process (midi, n, bank, settings, bpm, ppq);
            ppq += n * bpm / 60.0 / rate;

            auto* out = buffer.getWritePointer (0, start);
            int pos = 0;
            for (const auto m : midi)
            {
                if (m.samplePosition > pos)
                    engine.render (out + pos, m.samplePosition - pos);
                pos = m.samplePosition;
                const auto msg = m.getMessage();
                if (msg.isNoteOn())       engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
                else if (msg.isNoteOff()) engine.noteOff (msg.getNoteNumber());
            }
            if (pos < n)
                engine.render (out + pos, n - pos);
        }
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

        const float level = buffer.getRMSLevel (0, 0, numSamples);
        const float top = buffer.getMagnitude (0, 0, numSamples);
        std::cout << "Demo" << std::endl;
        check (level > 0.02f && top < 1.0f, "demo : du son, sans saturer (RMS " + juce::String (level, 3) + ", crete "
                                                + juce::String (top, 2) + ")");

        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (file), rate, 2, 24, {}, 0));
        if (writer == nullptr)
            return 1;
        writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
        return 0;
    }

    void bench()
    {
        constexpr double rate = 48000.0;
        lp303::AcidEngine engine;
        engine.prepare (rate, 256);
        engine.noteOn (36, 1.0f);
        std::vector<float> out (256);
        const int blocks = (int) (10.0 * rate / 256);
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < blocks; ++i)
            engine.render (out.data(), 256);
        const double ms = juce::Time::getMillisecondCounterHiRes() - start;
        std::cout << "10 s de son rendues en " << ms << " ms (" << (ms / 100.0) << " % d'un coeur), latence "
                  << engine.getLatencySamples() << " echantillons" << std::endl;
    }
}

int main (int argc, char* argv[])
{
    if (argc >= 2 && juce::String (argv[1]) == "--bench")
    {
        bench();
        return 0;
    }

    testFilterStability();
    testAliasing();
    testSlide();
    testAccent();
    testSequencer();
    testPatterns();

    if (argc >= 2 && renderDemo (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])) != 0)
        allOk = false;

    std::cout << (allOk ? "Tous les tests passent." : "Des tests ont echoue.") << std::endl;
    return allOk ? 0 : 1;
}
