// Rend une petite démo dans un fichier WAV et vérifie que le signal est sain
// (pas de NaN, pas de saturation numérique, du son quand on joue, du silence après).
//
// Utilisation : RenderTest <sortie.wav> [reference.f32]
//               RenderTest --bench   (charge processeur et latence)
//               RenderTest --render <scenario.txt> <sortie.wav>
//
// Un scénario décrit un rendu (utilisé pour comparer le moteur web au plugin) :
//   rate 44100 / duration 4 / param <identifiant du plugin> <valeur> / note 48 0.5 2.0
//
// Avec un fichier de référence : s'il n'existe pas, le rendu brut y est
// enregistré ; s'il existe, le rendu doit lui être identique. Ça garantit que
// les nouvelles options, réglées par défaut, ne changent pas le son.

#include <juce_audio_formats/juce_audio_formats.h>

#include <map>

#include "../Source/Arpeggiator.h"
#include "../Source/Effects.h"
#include "../Source/SynthEngine.h"

namespace
{
    struct Event
    {
        double time;
        std::function<void (modeld::SynthEngine&, modeld::Params&)> apply;
    };

    struct Stats
    {
        float peak = 0.0f;
        double sumSquares = 0.0;
        int count = 0;
        bool finite = true;

        void add (const float* data, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (data[i]))
                    finite = false;
                peak = std::max (peak, std::abs (data[i]));
                sumSquares += (double) data[i] * data[i];
            }
            count += n;
        }

        float rms() const { return count > 0 ? (float) std::sqrt (sumSquares / count) : 0.0f; }
    };

    // Fréquence d'auto-oscillation du filtre seul (passages à zéro sur 1 s).
    float selfOscillationHz (float cutoff, float rate)
    {
        modeld::detail::Ladder ladder;
        modeld::detail::Noise noise;
        const float g = std::tan (juce::MathConstants<float>::pi * cutoff / rate);
        const int n = (int) rate * 2;
        int crossings = 0;
        float previous = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float y = ladder.process (noise.white() * 1.0e-5f, g, 4.4f, false);
            if (i >= n / 2 && previous <= 0.0f && y > 0.0f)
                ++crossings;
            previous = y;
        }
        return (float) crossings;
    }

    // Gain (RMS sortie / RMS entrée) d'un sinus à 100 Hz, coupure à 2 kHz.
    float bassGain (float k, bool bassComp, float rate)
    {
        modeld::detail::Ladder ladder;
        const float g = std::tan (juce::MathConstants<float>::pi * 2000.0f / rate);
        const int n = (int) rate;
        double in2 = 0.0, out2 = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const float x = 0.2f * std::sin (juce::MathConstants<float>::twoPi * 100.0f * (float) i / rate);
            const float y = ladder.process (x, g, k, bassComp);
            if (i >= n / 2)
            {
                in2 += (double) x * x;
                out2 += (double) y * y;
            }
        }
        return (float) std::sqrt (out2 / in2);
    }

    // RMS d'une seconde de rendu après avoir joué des notes.
    float renderRms (const modeld::Params& params, std::initializer_list<int> notes, bool* finite = nullptr, float* peak = nullptr)
    {
        modeld::SynthEngine engine;
        engine.setParams (params);
        engine.prepare (48000.0, 256);
        for (int n : notes)
            engine.noteOn (n);

        Stats stats;
        std::vector<float> block (256);
        for (int i = 0; i < 48000 / 256; ++i)
        {
            engine.render (block.data(), 256);
            stats.add (block.data(), 256);
        }
        if (finite != nullptr) *finite = stats.finite;
        if (peak != nullptr)   *peak = stats.peak;
        return stats.rms();
    }

    // Notes émises par l'arpège en 2 s pour un accord do-mi-sol tenu.
    std::vector<int> arpNotes (const modeld::ArpSettings& settings)
    {
        modeld::Arpeggiator arp;
        arp.prepare (48000.0);
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

    // Position du premier écho d'une impulsion (canaux gauche et droit).
    std::pair<int, int> delayEchoes (const modeld::EffectsParams& params)
    {
        modeld::Effects fx;
        fx.prepare (48000.0);
        fx.setParams (params, 120.0, 1.0f);

        std::vector<float> l (48000, 0.0f), r (48000, 0.0f);
        l[0] = 1.0f;
        fx.process (l.data(), r.data(), 48000);

        auto firstEcho = [] (const std::vector<float>& x)
        {
            for (size_t i = 100; i < x.size(); ++i)
                if (std::abs (x[i]) > 0.3f)
                    return (int) i;
            return -1;
        };
        return { firstEcho (l), firstEcho (r) };
    }

    // Écart moyen entre gauche et droite d'un sinus passé dans le chorus.
    float chorusStereoWidth (int mode)
    {
        modeld::Effects fx;
        fx.prepare (48000.0);
        modeld::EffectsParams params;
        params.chorusMode = mode;
        fx.setParams (params, 120.0, 1.0f);

        std::vector<float> l (96000), r (96000);
        for (size_t i = 0; i < l.size(); ++i)
            l[i] = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * (float) i / 48000.0f);
        fx.process (l.data(), r.data(), (int) l.size());

        double diff = 0.0;
        for (size_t i = 48000; i < l.size(); ++i)
            diff += std::abs (l[i] - r[i]);
        return (float) (diff / 48000.0);
    }

    bool check (bool condition, const juce::String& what)
    {
        std::cout << (condition ? "  OK    " : "  ECHEC ") << what << std::endl;
        return condition;
    }

    // Réglages par identifiant du plugin, lus comme ModelDProcessor::readParams / readEffects.
    struct PluginSettings
    {
        std::map<juce::String, float> v {
            { "tune", 0 }, { "glide", 0 }, { "modMix", 0 }, { "oscMod", 0 }, { "osc3Ctrl", 1 },
            { "osc1Range", 3 }, { "osc1Wave", 2 }, { "osc2Range", 3 }, { "osc2Freq", 0.1f }, { "osc2Wave", 2 },
            { "osc3Range", 3 }, { "osc3Freq", -0.1f }, { "osc3Wave", 0 },
            { "osc1On", 1 }, { "osc1Level", 8 }, { "osc2On", 1 }, { "osc2Level", 8 }, { "osc3On", 0 }, { "osc3Level", 5 },
            { "noiseOn", 0 }, { "noiseLevel", 0 }, { "pinkNoise", 0 },
            { "cutoff", 1200 }, { "emphasis", 2 }, { "contour", 5 }, { "filterMod", 0 }, { "kbd1", 0 }, { "kbd2", 0 },
            { "bassComp", 0 }, { "fAttack", 0.005f }, { "fDecay", 0.4f }, { "fSustain", 3 },
            { "lAttack", 0.002f }, { "lDecay", 0.3f }, { "lSustain", 8 }, { "decay", 0 },
            { "volume", 7 }, { "bendRange", 2 }, { "drift", 3 }, { "retrigger", 0 }, { "poly", 0 },
            { "aftertouch", 0 }, { "velocity", 0 }, { "modAmount", 0 }, { "modFromLfo", 0 },
            { "osc1Freq", 0 }, { "osc2Fine", 0 }, { "osc3Fine", 0 }, { "osc1Rect", 0 }, { "osc2Rect", 0 }, { "osc3Rect", 0 },
            { "osc1Offset", 0 }, { "osc2Offset", 0 }, { "osc3Offset", 0 }, { "oscSync", 0 },
            { "lfoLevel", 0 }, { "lfoWave", 0 }, { "lfoRate", 2 }, { "lfoSync", 0 }, { "lfoDivision", 4 }, { "lfoDest", 0 },
            { "vco3Filter", 0 }, { "adsr", 0 }, { "fRelease", 0.3f }, { "lRelease", 0.3f },
            { "driveOn", 0 }, { "driveGain", 5 }, { "driveLowCut", 20 }, { "driveHighCut", 20000 },
            { "chorusMode", 0 }, { "chorusLevel", 10 }, { "autopan", 0 },
            { "delayOn", 0 }, { "delayLevel", 3 }, { "delayTime", 0.35f }, { "delaySync", 0 }, { "delayDivision", 5 },
            { "delayRight2x", 0 }, { "delayFeedback", 4 },
        };

        float get (const char* id) const { return v.at (id); }
        bool flag (const char* id) const { return get (id) > 0.5f; }
        int choice (const char* id) const { return juce::roundToInt (get (id)); }

        modeld::Params params() const
        {
            modeld::Params p;
            p.tune = get ("tune"); p.glide = get ("glide"); p.modMix = get ("modMix");
            p.oscMod = flag ("oscMod"); p.osc3Ctrl = flag ("osc3Ctrl");
            p.range = { choice ("osc1Range"), choice ("osc2Range"), choice ("osc3Range") };
            p.wave = { choice ("osc1Wave"), choice ("osc2Wave"), choice ("osc3Wave") };
            p.osc2Freq = get ("osc2Freq"); p.osc3Freq = get ("osc3Freq");
            p.oscOn = { flag ("osc1On"), flag ("osc2On"), flag ("osc3On") };
            p.oscLevel = { get ("osc1Level"), get ("osc2Level"), get ("osc3Level") };
            p.noiseOn = flag ("noiseOn"); p.noiseLevel = get ("noiseLevel"); p.pinkNoise = flag ("pinkNoise");
            p.cutoff = get ("cutoff"); p.emphasis = get ("emphasis"); p.contour = get ("contour");
            p.filterMod = flag ("filterMod"); p.kbd1 = flag ("kbd1"); p.kbd2 = flag ("kbd2"); p.bassComp = flag ("bassComp");
            p.filterAttack = get ("fAttack"); p.filterDecay = get ("fDecay"); p.filterSustain = get ("fSustain");
            p.loudAttack = get ("lAttack"); p.loudDecay = get ("lDecay"); p.loudSustain = get ("lSustain");
            p.decaySwitch = flag ("decay");
            p.volume = get ("volume"); p.bendRange = get ("bendRange"); p.drift = get ("drift");
            p.poly = flag ("poly"); p.retrigger = flag ("retrigger"); p.aftertouch = get ("aftertouch");
            p.velocity = get ("velocity"); p.modAmount = get ("modAmount"); p.modFromLfo = flag ("modFromLfo");
            p.osc1Freq = get ("osc1Freq"); p.osc2Fine = get ("osc2Fine"); p.osc3Fine = get ("osc3Fine");
            p.rectify = { flag ("osc1Rect"), flag ("osc2Rect"), flag ("osc3Rect") };
            p.offset = { get ("osc1Offset"), get ("osc2Offset"), get ("osc3Offset") };
            p.oscSync = flag ("oscSync");
            p.lfoLevel = get ("lfoLevel"); p.lfoWave = choice ("lfoWave"); p.lfoRate = get ("lfoRate");
            p.lfoSync = flag ("lfoSync"); p.lfoDivision = choice ("lfoDivision"); p.lfoDest = choice ("lfoDest");
            p.vco3Filter = get ("vco3Filter"); p.adsr = flag ("adsr");
            p.filterRelease = get ("fRelease"); p.loudRelease = get ("lRelease");
            return p;
        }

        modeld::EffectsParams effects() const
        {
            modeld::EffectsParams e;
            e.driveOn = flag ("driveOn"); e.driveGain = get ("driveGain");
            e.driveLowCut = get ("driveLowCut"); e.driveHighCut = get ("driveHighCut");
            e.chorusMode = choice ("chorusMode"); e.chorusLevel = get ("chorusLevel"); e.autopan = flag ("autopan");
            e.delayOn = flag ("delayOn"); e.delayLevel = get ("delayLevel"); e.delayTime = get ("delayTime");
            e.delaySync = flag ("delaySync"); e.delayDivision = choice ("delayDivision");
            e.delayRightDouble = flag ("delayRight2x"); e.delayFeedback = get ("delayFeedback");
            return e;
        }
    };

    int renderScenario (const juce::File& scenario, const juce::File& output)
    {
        PluginSettings settings;
        double rate = 44100.0, duration = 10.0;
        struct Event { juce::int64 sample; bool on; int note; };
        std::vector<Event> events;

        juce::StringArray lines;
        scenario.readLines (lines);
        for (const auto& line : lines)
        {
            const auto t = juce::StringArray::fromTokens (line.trim(), true);
            if (t.isEmpty()) continue;
            if (t[0] == "rate")          rate = t[1].getDoubleValue();
            else if (t[0] == "duration") duration = t[1].getDoubleValue();
            else if (t[0] == "note")
            {
                events.push_back ({ (juce::int64) std::round (t[2].getDoubleValue() * rate), true, t[1].getIntValue() });
                events.push_back ({ (juce::int64) std::round ((t[2].getDoubleValue() + t[3].getDoubleValue()) * rate), false, t[1].getIntValue() });
            }
            else if (t[0] == "param" && settings.v.count (t[1]) > 0) settings.v[t[1]] = t[2].getFloatValue();
            else { std::cerr << "ligne inconnue : " << line << std::endl; return 2; }
        }
        std::sort (events.begin(), events.end(), [] (const Event& a, const Event& b)
                   { return a.sample != b.sample ? a.sample < b.sample : ! a.on && b.on; });

        modeld::SynthEngine engine;
        const auto params = settings.params();
        engine.setParams (params);
        engine.prepare (rate, 256);
        modeld::Effects effects;
        effects.prepare (rate);
        const auto fx = settings.effects();
        effects.setParams (fx, 120.0, params.lfoSync ? (float) (120.0 / 60.0 / modeld::divisionBeats (params.lfoDivision)) : params.lfoRate);

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
        if (fx.anyOn())
            effects.process (left, audio.getWritePointer (1), total);
        else
            audio.copyFrom (1, 0, audio, 0, 0, total);

        output.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (output.createOutputStream().release());
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), rate, 2, 24, {}, 0));
        if (writer == nullptr) return 1;
        stream.release();
        writer->writeFromAudioSampleBuffer (audio, 0, total);
        return 0;
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
        // Charge processeur : temps de calcul / durée du son produit, à 48 kHz.
        auto bench = [] (bool poly, int numNotes)
        {
            modeld::Params p;
            p.poly = poly;
            p.oscOn = { true, true, true };
            modeld::SynthEngine engine;
            engine.setParams (p);
            engine.prepare (48000.0, 256);
            for (int i = 0; i < numNotes; ++i)
                engine.noteOn (48 + i * 3);

            std::vector<float> block (256);
            const int blocks = 10 * 48000 / 256;
            const auto start = juce::Time::getMillisecondCounterHiRes();
            for (int i = 0; i < blocks; ++i)
                engine.render (block.data(), 256);
            const double seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
            return 100.0 * seconds / (blocks * 256 / 48000.0);
        };

        modeld::SynthEngine engine;
        engine.prepare (48000.0, 256);
        std::cout << "Latence : " << engine.getLatencySamples() << " echantillons" << std::endl;
        std::cout << "Mono, 1 note      : " << juce::String (bench (false, 1), 2) << " % d'un coeur" << std::endl;
        std::cout << "Poly, 8 notes     : " << juce::String (bench (true, 8), 2) << " % d'un coeur" << std::endl;
        return 0;
    }

    if (argc < 2)
    {
        std::cerr << "usage: RenderTest <sortie.wav> [reference.f32]" << std::endl;
        return 2;
    }

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;
    constexpr double duration = 12.0;

    modeld::Params params;
    modeld::SynthEngine engine;
    engine.setParams (params);
    engine.prepare (sampleRate, blockSize);

    auto note = [] (int n, bool on) { return [n, on] (modeld::SynthEngine& e, modeld::Params&) { on ? e.noteOn (n) : e.noteOff (n); }; };

    // Une ligne de basse, un lead avec glide et vibrato, puis le filtre en auto-oscillation.
    std::vector<Event> events {
        { 0.0, [] (auto&, auto& p) { p.range = { 2, 2, 3 }; p.cutoff = 300.0f; p.emphasis = 5.0f; p.contour = 7.0f;
                                     p.filterDecay = 0.25f; p.filterSustain = 1.0f; } },
        { 0.00, note (36, true) }, { 0.20, note (36, false) },
        { 0.25, note (36, true) }, { 0.45, note (36, false) },
        { 0.50, note (48, true) }, { 0.70, note (48, false) },
        { 0.75, note (43, true) }, { 1.20, note (43, false) },
        { 1.50, note (39, true) }, { 1.90, note (39, false) },

        { 2.5, [] (auto&, auto& p) { p.range = { 3, 3, 0 }; p.glide = 0.12f; p.cutoff = 900.0f; p.emphasis = 3.0f;
                                     p.decaySwitch = true; p.loudDecay = 0.6f; p.oscMod = true; p.osc3Freq = 3.0f;
                                     p.osc3Ctrl = false; p.wave[2] = 0; } },
        { 2.5, [] (auto& e, auto&) { e.setModWheel (0.35f); } },
        { 2.50, note (60, true) }, { 3.00, note (67, true) }, { 3.40, note (67, false) },
        { 3.60, note (72, true) }, { 4.60, note (72, false) }, { 4.80, note (60, false) },
        { 5.0, [] (auto& e, auto&) { e.setModWheel (0.0f); } },

        { 6.0, [] (auto&, auto& p) { p.oscOn = { false, false, false }; p.emphasis = 10.0f; p.cutoff = 440.0f;
                                     p.contour = 0.0f; p.glide = 0.0f; p.oscMod = false; } },
        { 6.00, note (69, true) }, { 8.0, note (69, false) },

        // Tous les niveaux à fond pour vérifier que rien ne s'emballe.
        { 8.5, [] (auto&, auto& p) { p.oscOn = { true, true, true }; p.oscLevel = { 10.0f, 10.0f, 10.0f };
                                     p.noiseOn = true; p.noiseLevel = 10.0f; p.emphasis = 10.0f; p.cutoff = 20000.0f;
                                     p.volume = 10.0f; p.osc3Ctrl = true; p.range = { 4, 4, 4 }; } },
        { 8.50, note (84, true) }, { 10.0, note (84, false) },
    };

    const int totalSamples = (int) (duration * sampleRate);
    juce::AudioBuffer<float> output (1, totalSamples);

    Stats bass, selfOsc, loud, tail;
    size_t nextEvent = 0;

    for (int pos = 0; pos < totalSamples; pos += blockSize)
    {
        const int n = std::min (blockSize, totalSamples - pos);
        const double t = pos / sampleRate;

        while (nextEvent < events.size() && events[nextEvent].time <= t)
            events[nextEvent++].apply (engine, params);

        engine.setParams (params);

        float* out = output.getWritePointer (0, pos);
        engine.render (out, n);

        if (t < 2.0)                 bass.add (out, n);
        if (t > 6.5 && t < 8.0)      selfOsc.add (out, n);
        if (t > 8.5 && t < 10.0)     loud.add (out, n);
        if (t > 11.8)                tail.add (out, n);
    }

    std::cout << "Basse         : crete " << bass.peak << ", RMS " << bass.rms() << std::endl;
    std::cout << "Auto-oscill.  : crete " << selfOsc.peak << ", RMS " << selfOsc.rms() << std::endl;
    std::cout << "Tout a fond   : crete " << loud.peak << ", RMS " << loud.rms() << std::endl;
    std::cout << "Fin (silence) : crete " << tail.peak << std::endl;

    bool ok = true;
    ok &= check (bass.finite && selfOsc.finite && loud.finite && tail.finite, "aucun NaN / infini");
    ok &= check (bass.rms() > 0.02f, "la basse produit du son");
    ok &= check (selfOsc.rms() > 0.02f, "le filtre auto-oscille a emphasis 10 sans oscillateurs");
    ok &= check (loud.peak < 2.0f, "pas d'emballement avec tout a fond");
    ok &= check (tail.peak < 1.0e-3f, "silence apres relachement");

    // Le filtre seul, au double de 48 kHz comme dans le moteur.
    constexpr float filterRate = 96000.0f;
    for (float cutoff : { 220.0f, 880.0f, 3000.0f })
    {
        const float hz = selfOscillationHz (cutoff, filterRate);
        ok &= check (std::abs (hz / cutoff - 1.0f) < 0.03f,
                     "auto-oscillation accordee sur la coupure " + juce::String (cutoff, 0) + " Hz (mesure "
                         + juce::String (hz, 0) + " Hz)");
    }

    const float lossOriginal = bassGain (3.0f, false, filterRate) / bassGain (0.0f, false, filterRate);
    const float lossCompensated = bassGain (3.0f, true, filterRate) / bassGain (0.0f, true, filterRate);
    ok &= check (lossOriginal < 0.35f, "comme l'original, la resonance creuse les graves (gain "
                                           + juce::String (juce::Decibels::gainToDecibels (lossOriginal), 1) + " dB)");
    ok &= check (lossCompensated > 0.45f, "BASS COMP limite la perte (gain "
                                              + juce::String (juce::Decibels::gainToDecibels (lossCompensated), 1) + " dB)");

    //--------------------------------------------------------------------------
    // Options supplémentaires
    {
        modeld::Params poly;
        poly.poly = true;
        const float single = renderRms (poly, { 60 });
        const float chord = renderRms (poly, { 60, 64, 67 });
        ok &= check (chord > single * 1.3f, "POLY : un accord de 3 notes joue bien 3 voix (RMS "
                                                + juce::String (single, 3) + " -> " + juce::String (chord, 3) + ")");

        modeld::ArpSettings arp;
        arp.on = true;
        arp.division = 6;   // 1/16 à 120 BPM : 8 notes par seconde
        auto notes = arpNotes (arp);
        ok &= check (notes.size() == 16, "arpege 1/16 a 120 BPM : 16 notes en 2 s (" + juce::String ((int) notes.size()) + ")");
        ok &= check (notes.size() >= 6 && notes[0] == 60 && notes[1] == 64 && notes[2] == 67 && notes[3] == 60,
                     "arpege Up : do, mi, sol, do...");

        arp.mode = 1;
        notes = arpNotes (arp);
        ok &= check (notes.size() >= 3 && notes[0] == 67 && notes[1] == 64 && notes[2] == 60, "arpege Down : sol, mi, do");

        arp.mode = 0;
        arp.range = 2;
        notes = arpNotes (arp);
        ok &= check (notes.size() >= 7 && notes[3] == 72 && notes[4] == 76 && notes[5] == 79 && notes[6] == 60,
                     "arpege RANGE 2 octaves : do mi sol, puis une octave au-dessus, puis retour");
        arp.range = 1;

        arp.gate = true;
        for (size_t i = 0; i < 16; ++i)
            arp.steps[i] = i % 2 == 0;
        notes = arpNotes (arp);
        ok &= check (notes.size() == 8, "motif GATE un pas sur deux : 8 notes en 2 s (" + juce::String ((int) notes.size()) + ")");

        modeld::EffectsParams delay;
        delay.delayOn = true;
        delay.delayLevel = 10.0f;
        delay.delayFeedback = 0.0f;
        delay.delayTime = 0.25f;
        auto [echoL, echoR] = delayEchoes (delay);
        ok &= check (std::abs (echoL - 12000) <= 2 && std::abs (echoR - 12000) <= 2,
                     "delay 250 ms : echo a 12000 echantillons (" + juce::String (echoL) + ")");

        delay.delaySync = true;
        delay.delayDivision = 5;   // 1/8 à 120 BPM = 250 ms
        delay.delayTime = 1.0f;
        delay.delayRightDouble = true;
        std::tie (echoL, echoR) = delayEchoes (delay);
        ok &= check (std::abs (echoL - 12000) <= 2 && std::abs (echoR - 6000) <= 2,
                     "delay synchro 1/8 + RIGHT 2X : gauche 250 ms, droite 125 ms");

        const float widthI = chorusStereoWidth (1), widthII = chorusStereoWidth (2), widthBoth = chorusStereoWidth (3);
        ok &= check (widthI > 0.01f && widthII > 0.01f, "chorus I et II : stereo (gauche et droite modulees en opposition)");
        ok &= check (widthBoth < 1.0e-6f, "chorus I+II : mono, comme sur le Juno-60");

        // Tout à fond en même temps : rien ne doit exploser.
        modeld::Params wild;
        wild.poly = true;
        wild.oscSync = true;
        wild.rectify = { true, true, true };
        wild.offset = { 0.5f, -0.5f, 0.3f };
        wild.oscOn = { true, true, true };
        wild.oscLevel = { 10.0f, 10.0f, 10.0f };
        wild.lfoLevel = 10.0f;
        wild.lfoDest = 2;
        wild.vco3Filter = 10.0f;
        wild.modAmount = 10.0f;
        wild.oscMod = wild.filterMod = true;
        wild.emphasis = 10.0f;
        wild.volume = 10.0f;
        wild.lfoDest = 0;
        bool finite = true;
        float peak = 0.0f;
        renderRms (wild, { 36, 48, 60, 64, 67, 72, 76, 79 }, &finite, &peak);
        ok &= check (finite && peak < 4.0f, "toutes les options a fond, 8 voix : signal sain (crete "
                                                + juce::String (peak, 2) + ")");
    }

    if (argc >= 3)
    {
        const auto cwd = juce::File::getCurrentWorkingDirectory();
        const juce::File reference (cwd.getChildFile (argv[2]));
        const auto* samples = output.getReadPointer (0);
        const size_t numBytes = (size_t) totalSamples * sizeof (float);

        if (! reference.existsAsFile())
        {
            reference.replaceWithData (samples, numBytes);
            std::cout << "Reference enregistree : " << reference.getFullPathName() << std::endl;
        }
        else
        {
            juce::MemoryBlock data;
            reference.loadFileAsData (data);
            float maxDiff = data.getSize() == numBytes ? 0.0f : 1.0f;
            if (data.getSize() == numBytes)
                for (int i = 0; i < totalSamples; ++i)
                    maxDiff = std::max (maxDiff, std::abs (samples[i] - static_cast<const float*> (data.getData())[i]));

            ok &= check (maxDiff < 1.0e-5f, "son identique a la reference (ecart max " + juce::String (maxDiff, 8) + ")");
        }
    }

    // Écriture du WAV (24 bits) pour écouter le résultat.
    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), sampleRate, 1, 24, {}, 0));
    if (writer != nullptr)
    {
        stream.release();
        output.applyGain (0.8f);
        writer->writeFromAudioSampleBuffer (output, 0, totalSamples);
        std::cout << "WAV ecrit : " << file.getFullPathName() << std::endl;
    }

    return ok ? 0 : 1;
}
