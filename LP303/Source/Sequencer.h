#pragma once

// Séquenceur pas à pas de LP-303 : 8 motifs de 16 pas (longueur réglable de 1
// à 16), en doubles croches calées sur le tempo et la lecture de l'hôte.
//
// Chaque pas a une note (do à do de l'octave au-dessus), un décalage d'octave
// (haut / bas), un accent, un slide, et peut être un silence. Comme sur la
// machine d'origine :
//   - une note normale dure une demi-double croche ;
//   - un slide lie le pas à son suivant : la porte reste ouverte et la hauteur
//     glisse vers la note suivante, sans redéclencher les enveloppes ;
//   - un changement de motif attend la fin du motif en cours.
//
// Le séquenceur produit des messages MIDI ordinaires (vélocité 127 pour un
// accent, notes qui se chevauchent pour un slide) : le moteur les joue
// exactement comme un clavier.

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cmath>
#include <limits>

namespace lp303
{

constexpr int numPatterns = 8;
constexpr int numSteps = 16;
constexpr int baseNote = 36;        // note 0 du motif : do 1 (C1, 65 Hz)

struct Step
{
    int note = 0;           // 0 à 12 : do, do#... si, do aigu
    int octave = 0;         // -1 = bas, 0, +1 = haut
    bool accent = false;
    bool slide = false;
    bool gate = true;       // faux = silence

    int midiNote (int transpose) const noexcept
    {
        return juce::jlimit (0, 127, baseNote + note + 12 * octave + transpose);
    }

    uint32_t pack() const noexcept
    {
        return (uint32_t) juce::jlimit (0, 12, note)
             | ((uint32_t) (juce::jlimit (-1, 1, octave) + 1) << 4)
             | (accent ? 1u << 6 : 0u)
             | (slide ? 1u << 7 : 0u)
             | (gate ? 1u << 8 : 0u);
    }

    static Step unpack (uint32_t v) noexcept
    {
        Step s;
        s.note = juce::jlimit (0, 12, (int) (v & 15u));
        s.octave = juce::jlimit (-1, 1, (int) ((v >> 4) & 3u) - 1);
        s.accent = ((v >> 6) & 1u) != 0;
        s.slide = ((v >> 7) & 1u) != 0;
        s.gate = ((v >> 8) & 1u) != 0;
        return s;
    }

    bool operator== (const Step& o) const noexcept { return pack() == o.pack(); }
    bool operator!= (const Step& o) const noexcept { return pack() != o.pack(); }
};

//==============================================================================
// Les motifs, lisibles par le thread audio et modifiables par l'interface sans
// verrou (chaque pas tient dans un entier atomique).
class PatternBank
{
public:
    PatternBank()
    {
        for (int p = 0; p < numPatterns; ++p)
            clear (p);
    }

    Step get (int pattern, int step) const noexcept
    {
        return Step::unpack (steps[index (pattern, step)].load (std::memory_order_relaxed));
    }

    void set (int pattern, int step, const Step& s) noexcept
    {
        steps[index (pattern, step)].store (s.pack(), std::memory_order_relaxed);
    }

    int getLength (int pattern) const noexcept
    {
        return lengths[(size_t) juce::jlimit (0, numPatterns - 1, pattern)].load (std::memory_order_relaxed);
    }

    void setLength (int pattern, int length) noexcept
    {
        lengths[(size_t) juce::jlimit (0, numPatterns - 1, pattern)].store (juce::jlimit (1, numSteps, length),
                                                                          std::memory_order_relaxed);
    }

    void clear (int pattern) noexcept
    {
        for (int s = 0; s < numSteps; ++s)
            set (pattern, s, Step {});
        setLength (pattern, numSteps);
    }

    // Texte compact pour la sauvegarde : « 16|0,0,AS 3,1, 0,0,R ... »
    // (note, octave, puis A = accent, S = slide, R = silence).
    juce::String toString (int pattern) const
    {
        juce::StringArray tokens;
        for (int s = 0; s < numSteps; ++s)
        {
            const auto st = get (pattern, s);
            juce::String flags;
            if (st.accent) flags << "A";
            if (st.slide)  flags << "S";
            if (! st.gate) flags << "R";
            tokens.add (juce::String (st.note) + "," + juce::String (st.octave) + "," + flags);
        }
        return juce::String (getLength (pattern)) + "|" + tokens.joinIntoString (" ");
    }

    bool fromString (int pattern, const juce::String& text)
    {
        if (! text.containsChar ('|'))
            return false;

        const auto tokens = juce::StringArray::fromTokens (text.fromFirstOccurrenceOf ("|", false, false), " ", "");
        if (tokens.size() != numSteps)
            return false;

        setLength (pattern, text.upToFirstOccurrenceOf ("|", false, false).getIntValue());
        for (int s = 0; s < numSteps; ++s)
        {
            const auto parts = juce::StringArray::fromTokens (tokens[s], ",", "");
            Step st;
            st.note = parts[0].getIntValue();
            st.octave = parts[1].getIntValue();
            st.accent = parts[2].containsChar ('A');
            st.slide = parts[2].containsChar ('S');
            st.gate = ! parts[2].containsChar ('R');
            set (pattern, s, Step::unpack (st.pack()));    // bornes vérifiées
        }
        return true;
    }

private:
    static size_t index (int pattern, int step) noexcept
    {
        return (size_t) (juce::jlimit (0, numPatterns - 1, pattern) * numSteps + juce::jlimit (0, numSteps - 1, step));
    }

    std::array<std::atomic<uint32_t>, (size_t) (numPatterns * numSteps)> steps;
    std::array<std::atomic<int>, (size_t) numPatterns> lengths;
};

//==============================================================================
struct SequencerSettings
{
    bool running = false;   // séquenceur en marche et transport en lecture
    int pattern = 0;        // motif demandé (il démarre à la fin du motif en cours)
    float swing = 0.0f;     // 0 = droit, 1 = shuffle maximal (75 %)
    int transpose = 0;      // demi-tons
};

class Sequencer
{
public:
    static constexpr double gateLength = 0.5;     // fraction du pas pendant laquelle la note sonne
    static constexpr int normalVelocity = 90;
    static constexpr int accentVelocity = 127;

    void prepare (double sampleRate) noexcept
    {
        rate = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        sounding = -1;
        gateOffPending = false;
        tieFromPrevious = false;
        wasRunning = false;
        needsResync = true;
        playingStep = -1;
    }

    // Ajoute au tampon `out` les notes d'un bloc de numSamples échantillons dont
    // le premier tombe à la position ppq (en noires) de l'hôte.
    void process (juce::MidiBuffer& out, int numSamples, const PatternBank& bank, const SequencerSettings& settings,
                  double bpm, double ppq) noexcept
    {
        if (! settings.running || numSamples <= 0 || bpm <= 0.0)
        {
            stop (out);
            activePattern = settings.pattern;
            return;
        }

        const double ppqPerSample = bpm / 60.0 / rate;
        const double end = ppq + numSamples * ppqPerSample;
        swingOffset = 0.125 * (double) juce::jlimit (0.0f, 1.0f, settings.swing);

        // Lecture qui démarre, ou saut de la tête de lecture (boucle, clic sur
        // la règle) : on repart de la grille, au premier pas qui suit.
        const bool jumped = ! wasRunning || std::abs (ppq - expectedPpq) > juce::jmax (1.0e-4, 4.0 * ppqPerSample);
        if (jumped)
        {
            if (sounding >= 0)
                out.addEvent (juce::MidiMessage::noteOff (1, sounding), 0);
            sounding = -1;
            gateOffPending = false;
            tieFromPrevious = false;
            needsResync = true;
            if (! wasRunning)
                activePattern = settings.pattern;

            lastStep = (juce::int64) std::floor (ppq * 4.0) - 2;
            while (stepStart (lastStep + 1) < ppq - 1.0e-9)
                ++lastStep;
        }
        wasRunning = true;
        expectedPpq = end;

        for (;;)
        {
            const juce::int64 k = lastStep + 1;
            const double tStep = stepStart (k);
            const double tGate = gateOffPending ? gateOffPpq : std::numeric_limits<double>::max();
            const double t = juce::jmin (tStep, tGate);

            const int sample = juce::jmax (0, (int) std::ceil ((t - ppq) / ppqPerSample - 1.0e-6));
            if (sample >= numSamples)
                break;

            if (tGate <= tStep)
            {
                if (sounding >= 0)
                    out.addEvent (juce::MidiMessage::noteOff (1, sounding), sample);
                sounding = -1;
                gateOffPending = false;
            }
            else
            {
                playStep (out, k, tStep, sample, bank, settings);
                lastStep = k;
            }
        }
    }

    // Arrêt : la note en cours se coupe.
    void stop (juce::MidiBuffer& out) noexcept
    {
        if (sounding >= 0)
            out.addEvent (juce::MidiMessage::noteOff (1, sounding), 0);
        reset();
    }

    int getPlayingStep() const noexcept    { return playingStep.load(); }
    int getActivePattern() const noexcept  { return activePattern.load(); }

private:
    double stepStart (juce::int64 k) const noexcept
    {
        return (double) k * 0.25 + ((k & 1) != 0 ? swingOffset : 0.0);
    }

    void playStep (juce::MidiBuffer& out, juce::int64 k, double tStep, int sample,
                   const PatternBank& bank, const SequencerSettings& settings) noexcept
    {
        int pattern = activePattern.load();
        int position;

        if (needsResync)
        {
            const int length = bank.getLength (pattern);
            position = (int) (((k % length) + length) % length);
            needsResync = false;
        }
        else
        {
            position = nextPosition;
        }

        // Le motif demandé démarre en tête de cycle.
        if (position == 0 && pattern != settings.pattern)
        {
            pattern = settings.pattern;
            activePattern = pattern;
        }

        const int length = bank.getLength (pattern);
        if (position >= length)
            position = 0;
        nextPosition = (position + 1) % length;

        const auto step = bank.get (pattern, position);
        const bool tie = tieFromPrevious && sounding >= 0;

        if (step.gate)
        {
            const int note = step.midiNote (settings.transpose);
            const auto velocity = (juce::uint8) (step.accent ? accentVelocity : normalVelocity);

            if (tie)
            {
                // Slide : la nouvelle note arrive avant que l'ancienne ne s'arrête.
                if (note != sounding)
                {
                    out.addEvent (juce::MidiMessage::noteOn (1, note, velocity), sample);
                    out.addEvent (juce::MidiMessage::noteOff (1, sounding), sample);
                }
            }
            else
            {
                if (sounding >= 0)
                    out.addEvent (juce::MidiMessage::noteOff (1, sounding), sample);
                out.addEvent (juce::MidiMessage::noteOn (1, note, velocity), sample);
            }

            sounding = note;
            tieFromPrevious = step.slide;
            gateOffPending = ! step.slide;
            gateOffPpq = tStep + gateLength * (stepStart (k + 1) - tStep);
        }
        else
        {
            if (sounding >= 0)
                out.addEvent (juce::MidiMessage::noteOff (1, sounding), sample);
            sounding = -1;
            tieFromPrevious = false;
            gateOffPending = false;
        }

        playingStep = position;
    }

    double rate = 44100.0;
    double swingOffset = 0.0;
    double expectedPpq = 0.0;
    double gateOffPpq = 0.0;
    juce::int64 lastStep = 0;
    int nextPosition = 0;
    int sounding = -1;
    bool gateOffPending = false, tieFromPrevious = false, wasRunning = false, needsResync = true;

    std::atomic<int> playingStep { -1 };
    std::atomic<int> activePattern { 0 };
};

} // namespace lp303
