#pragma once

// Arpégiateur : remplace les notes tenues par une suite de notes calée sur le
// tempo. Avec GATE, un motif de 16 pas décide quels pas jouent.

#include <juce_audio_basics/juce_audio_basics.h>

#include "SynthEngine.h"

#include <algorithm>
#include <limits>

namespace modeld
{

struct ArpSettings
{
    bool on = false;
    int mode = 0;               // Up, Down, Up/Down, Random, Played
    int division = 5;           // index dans divisionBeats()
    int range = 1;              // nombre d'octaves parcourues (1 à 4)
    bool gate = false;          // applique le motif de 16 pas
    std::array<bool, 16> steps {};
};

class Arpeggiator
{
public:
    static juce::StringArray modeNames() { return { "Up", "Down", "Up/Down", "Random", "Played" }; }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        output.ensureSize (2048);
        numHeld = 0;
        currentNote = -1;
        noteOffCountdown = -1;
        samplesToNextStep = 0.0;
        stepIndex = sequenceIndex = 0;
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
                // On coupe la note en cours ; les touches tenues ne sonnent qu'au prochain appui.
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

        const double stepBeats = divisionBeats (s.division);
        stepSamples = juce::jmax (1.0, stepBeats * 60.0 / juce::jlimit (20.0, 400.0, bpm) * sampleRate);
        gateSamples = juce::jmax (1, (int) (stepSamples * 0.75));

        // Hôte en lecture : les pas tombent sur la grille de la mesure.
        if (hostPlaying)
        {
            const double position = ppqPosition / stepBeats;
            const double next = std::ceil (position - 1.0e-9);
            samplesToNextStep = (next - position) * stepSamples;
            stepIndex = ((int) next % 16 + 16) % 16;
        }
        syncedToHost = hostPlaying;

        int position = 0;
        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            const int time = juce::jlimit (0, numSamples - 1, metadata.samplePosition);
            advance (position, time);
            position = time;

            if (message.isNoteOn())
                press (message.getNoteNumber(), message.getVelocity());
            else if (message.isNoteOff())
                release (message.getNoteNumber());
            else
                output.addEvent (message, time);
        }
        advance (position, numSamples);

        midi.swapWith (output);
    }

private:
    void press (int note, juce::uint8 velocity)
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] == note)
                return;

        if (numHeld == 0 && ! syncedToHost)
        {
            // Sans hôte en lecture, l'arpège démarre au moment où l'on joue.
            samplesToNextStep = 0.0;
            stepIndex = 0;
            sequenceIndex = 0;
        }

        if (numHeld < (int) held.size())
        {
            held[(size_t) numHeld++] = note;
            lastVelocity = velocity;
        }
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
        const int step = stepIndex;
        stepIndex = (stepIndex + 1) % 16;

        if (numHeld == 0)
            return;

        if (currentNote >= 0)
        {
            output.addEvent (juce::MidiMessage::noteOff (1, currentNote), time);
            currentNote = -1;
        }

        if (s.gate && ! s.steps[(size_t) step])
            return;

        currentNote = pickNote();
        output.addEvent (juce::MidiMessage::noteOn (1, currentNote, lastVelocity), time);
        noteOffCountdown = gateSamples;
    }

    int pickNote()
    {
        // Notes tenues (triées, ou dans l'ordre joué), répétées sur RANGE octaves.
        std::array<int, 32> base {};
        std::copy (held.begin(), held.begin() + numHeld, base.begin());
        if (s.mode != 4)
            std::sort (base.begin(), base.begin() + numHeld);

        std::array<int, 128> notes {};
        int count = 0;
        for (int octave = 0; octave < juce::jlimit (1, 4, s.range); ++octave)
            for (int i = 0; i < numHeld && count < (int) notes.size(); ++i)
                if (base[(size_t) i] + 12 * octave <= 127)
                    notes[(size_t) count++] = base[(size_t) i] + 12 * octave;

        const int i = sequenceIndex++;

        switch (s.mode)
        {
            case 1:  return notes[(size_t) (count - 1 - i % count)];
            case 2:
            {
                if (count == 1)
                    return notes[0];
                const int period = 2 * count - 2;
                const int k = i % period;
                return notes[(size_t) (k < count ? k : period - k)];
            }
            case 3:  return notes[(size_t) random.nextInt (count)];
            default: return notes[(size_t) (i % count)];   // Up, et Played dans l'ordre joué
        }
    }

    ArpSettings s;
    double sampleRate = 44100.0, stepSamples = 1.0, samplesToNextStep = 0.0;
    int gateSamples = 1;
    juce::MidiBuffer output;

    std::array<int, 32> held {};
    int numHeld = 0;
    juce::uint8 lastVelocity = 100;

    int currentNote = -1, noteOffCountdown = -1;
    int stepIndex = 0, sequenceIndex = 0;
    bool wasOn = false, syncedToHost = false;
    juce::Random random;
};

} // namespace modeld
