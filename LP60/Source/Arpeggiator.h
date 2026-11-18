#pragma once

// Arpégiateur du Juno-60 : UP, UP & DOWN, DOWN sur 1, 2 ou 3 octaves.
// Vitesse libre de 1,5 à 50 pas par seconde (notes de service), ou calée sur
// le tempo de l'hôte en option.

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace lp60
{

struct ArpSettings
{
    bool on = false;
    int mode = 0;           // 0 = UP, 1 = UP & DOWN, 2 = DOWN
    int range = 1;          // 1 à 3 octaves
    float rate = 5.0f;      // 0..10
    bool sync = false;      // vitesse calée sur le tempo
};

class Arpeggiator
{
public:
    // Valeurs de note quand la vitesse est synchronisée, de la plus lente à la plus rapide.
    static const juce::StringArray& syncNames()
    {
        static const juce::StringArray names { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
        return names;
    }

    static double stepsPerSecond (const ArpSettings& s, double bpm) noexcept
    {
        const float r = juce::jlimit (0.0f, 10.0f, s.rate);
        if (! s.sync)
            return 1.5 * std::pow (50.0 / 1.5, r / 10.0);

        static constexpr double beats[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        const int index = juce::roundToInt (r / 10.0f * 5.0f);
        return juce::jlimit (20.0, 400.0, bpm) / 60.0 / beats[index];
    }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        output.ensureSize (2048);
        numHeld = 0;
        currentNote = -1;
        noteOffCountdown = -1;
        samplesToNextStep = 0.0;
        sequenceIndex = 0;
        wasOn = false;
    }

    // Remplace les notes de midi par celles de l'arpège. Les autres messages passent.
    void process (juce::MidiBuffer& midi, int numSamples, const ArpSettings& settings,
                  double bpm, bool hostPlaying, double ppqPosition)
    {
        if (! settings.on)
        {
            if (wasOn)
            {
                if (currentNote >= 0)
                    midi.addEvent (juce::MidiMessage::noteOff (1, currentNote), 0);
                currentNote = -1;
                noteOffCountdown = -1;
                numHeld = 0;
                wasOn = false;
            }
            return;
        }

        s = settings;
        wasOn = true;
        output.clear();

        const double perSecond = stepsPerSecond (s, bpm);
        stepSamples = juce::jmax (1.0, sampleRate / perSecond);
        gateSamples = juce::jmax (1, (int) (stepSamples * 0.5));

        // En synchro avec l'hôte en lecture, les pas tombent sur la grille.
        syncedToHost = s.sync && hostPlaying;
        if (syncedToHost)
        {
            const double stepBeats = juce::jlimit (20.0, 400.0, bpm) / 60.0 / perSecond;
            const double position = ppqPosition / stepBeats;
            samplesToNextStep = (std::ceil (position - 1.0e-9) - position) * stepSamples;
        }

        int position = 0;
        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            const int time = juce::jlimit (0, numSamples - 1, metadata.samplePosition);
            advance (position, time);
            position = time;

            if (message.isNoteOn())
                press (message.getNoteNumber());
            else if (message.isNoteOff())
                release (message.getNoteNumber());
            else
                output.addEvent (message, time);
        }
        advance (position, numSamples);

        midi.swapWith (output);
    }

private:
    void press (int note)
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] == note)
                return;

        if (numHeld == 0 && ! syncedToHost)
        {
            // Sans synchro, l'arpège démarre au moment où l'on joue.
            samplesToNextStep = 0.0;
            sequenceIndex = 0;
        }

        if (numHeld < (int) held.size())
            held[(size_t) numHeld++] = note;
    }

    void release (int note)
    {
        for (int i = 0; i < numHeld; ++i)
        {
            if (held[(size_t) i] == note)
            {
                for (int j = i; j < numHeld - 1; ++j)
                    held[(size_t) j] = held[(size_t) j + 1];
                --numHeld;
                return;
            }
        }
    }

    // Émet les pas et les fins de notes qui tombent dans [from, to).
    void advance (int from, int to)
    {
        while (from < to)
        {
            const int nextOff = noteOffCountdown >= 0 ? from + noteOffCountdown : std::numeric_limits<int>::max();
            const int nextStep = from + juce::jmax (0, (int) std::ceil (samplesToNextStep - 1.0e-9));
            const int t = juce::jmin (nextOff, nextStep);

            if (t >= to)
            {
                consume (to - from);
                return;
            }

            consume (t - from);
            from = t;

            if (t == nextOff && currentNote >= 0)
            {
                output.addEvent (juce::MidiMessage::noteOff (1, currentNote), t);
                currentNote = -1;
                noteOffCountdown = -1;
            }

            if (t == nextStep)
            {
                playStep (t);
                samplesToNextStep += stepSamples;
            }
        }
    }

    void consume (int samples)
    {
        samplesToNextStep -= samples;
        if (noteOffCountdown >= 0)
            noteOffCountdown -= samples;
    }

    void playStep (int time)
    {
        if (numHeld == 0)
            return;

        if (currentNote >= 0)
            output.addEvent (juce::MidiMessage::noteOff (1, currentNote), time);

        currentNote = pickNote();
        output.addEvent (juce::MidiMessage::noteOn (1, currentNote, (juce::uint8) 100), time);
        noteOffCountdown = gateSamples;
    }

    int pickNote()
    {
        std::array<int, 32> base {};
        std::copy (held.begin(), held.begin() + numHeld, base.begin());
        std::sort (base.begin(), base.begin() + numHeld);

        std::array<int, 96> notes {};
        int count = 0;
        for (int octave = 0; octave < juce::jlimit (1, 3, s.range); ++octave)
            for (int i = 0; i < numHeld && count < (int) notes.size(); ++i)
                if (base[(size_t) i] + 12 * octave <= 127)
                    notes[(size_t) count++] = base[(size_t) i] + 12 * octave;

        const int i = sequenceIndex++;

        switch (s.mode)
        {
            case 1:
            {
                if (count == 1)
                    return notes[0];
                const int period = 2 * count - 2;
                const int k = i % period;
                return notes[(size_t) (k < count ? k : period - k)];
            }
            case 2:  return notes[(size_t) (count - 1 - i % count)];
            default: return notes[(size_t) (i % count)];
        }
    }

    ArpSettings s;
    double sampleRate = 44100.0, stepSamples = 1.0, samplesToNextStep = 0.0;
    int gateSamples = 1;
    juce::MidiBuffer output;

    std::array<int, 32> held {};
    int numHeld = 0;

    int currentNote = -1, noteOffCountdown = -1, sequenceIndex = 0;
    bool wasOn = false, syncedToHost = false;
};

} // namespace lp60
