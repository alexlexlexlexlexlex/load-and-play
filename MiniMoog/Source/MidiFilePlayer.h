#pragma once

// Lecteur de fichier MIDI : on glisse un .mid sur la fenêtre, un petit panneau
// apparaît (tempo, lecture / pause, retour au début, boucle) et le synthé joue
// le fichier comme s'il était joué au clavier. Sans fichier, rien n'apparaît.
//
// Fichier identique dans LP-60 et Model Play.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <vector>

//==============================================================================
// Partie audio. Les commandes (lecture, tempo...) viennent de l'interface par
// des variables atomiques ; le fichier est échangé sous verrou, et le thread
// audio n'attend jamais ce verrou.
class MidiFilePlayer
{
public:
    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate; }

    // Charge le fichier. Rend false s'il est illisible ou ne contient aucune note.
    bool load (const juce::File& file)
    {
        juce::FileInputStream stream (file);
        juce::MidiFile midi;
        if (! stream.openedOk() || ! midi.readFrom (stream))
            return false;

        // Temps en noires : le tempo choisi dans le panneau s'applique au fichier entier.
        const short format = midi.getTimeFormat();
        double fileTempo = 120.0;
        if (format <= 0)
            midi.convertTimestampTicksToSeconds();

        std::vector<Event> loaded;
        for (int t = 0; t < midi.getNumTracks(); ++t)
        {
            for (const auto* holder : *midi.getTrack (t))
            {
                const auto& m = holder->message;
                const double beat = format > 0 ? m.getTimeStamp() / format : m.getTimeStamp() * 2.0;

                if (m.isTempoMetaEvent() && beat <= 0.0)
                    fileTempo = 60.0 / m.getTempoSecondsPerQuarterNote();

                if (m.isNoteOnOrOff() || m.isController() || m.isPitchWheel() || m.isChannelPressure())
                {
                    auto copy = m;
                    copy.setChannel (1);
                    loaded.push_back ({ beat, copy });
                }
            }
        }

        if (std::none_of (loaded.begin(), loaded.end(), [] (const Event& e) { return e.message.isNoteOn(); }))
            return false;

        // Note-off avant note-on au même instant, pour ne pas couper une note rejouée.
        std::stable_sort (loaded.begin(), loaded.end(), [] (const Event& a, const Event& b)
        {
            if (a.beat != b.beat)
                return a.beat < b.beat;
            return a.message.isNoteOff() && ! b.message.isNoteOff();
        });

        // La boucle dure jusqu'à la fin de la mesure (4/4) qui contient le dernier événement.
        const double lastBeat = loaded.back().beat;
        const double length = juce::jmax (4.0, std::ceil ((lastBeat + 1.0e-6) / 4.0) * 4.0);

        {
            const juce::SpinLock::ScopedLockType lock (eventsLock);
            events = std::move (loaded);
            lengthBeats = length;
            position = 0.0;
            nextEvent = 0;
        }

        fileName = file.getFileNameWithoutExtension();
        tempo = juce::jlimit (40.0, 240.0, std::round (fileTempo));
        playing = false;
        flushRequested = true;
        loadedFlag = true;
        return true;
    }

    void unload()
    {
        playing = false;
        flushRequested = true;
        loadedFlag = false;
        const juce::SpinLock::ScopedLockType lock (eventsLock);
        events.clear();
    }

    // Commandes de l'interface.
    void setPlaying (bool shouldPlay) noexcept { playing = shouldPlay; if (! shouldPlay) flushRequested = true; }
    void restart() noexcept                    { restartRequested = true; }
    void setLooping (bool shouldLoop) noexcept { looping = shouldLoop; }
    void setTempo (double bpm) noexcept        { tempo = juce::jlimit (40.0, 240.0, bpm); }

    bool isLoaded() const noexcept   { return loadedFlag; }
    bool isPlaying() const noexcept  { return playing; }
    bool isLooping() const noexcept  { return looping; }
    double getTempo() const noexcept { return tempo; }
    juce::String getFileName() const { return fileName; }

    // Ajoute au tampon MIDI les événements du fichier qui tombent dans ce bloc.
    void process (juce::MidiBuffer& midi, int numSamples)
    {
        const juce::SpinLock::ScopedTryLockType lock (eventsLock);
        if (! lock.isLocked())
            return;

        if (flushRequested.exchange (false))
            releaseNotes (midi, 0);

        if (restartRequested.exchange (false))
        {
            releaseNotes (midi, 0);
            seek (0.0);
        }

        if (! playing || events.empty() || sampleRate <= 0.0)
            return;

        const double beatsPerSample = tempo / 60.0 / sampleRate;
        int sample = 0, wraps = 0;

        while (sample < numSamples)
        {
            // Prochain événement, ou fin du fichier, dans ce bloc ?
            const double blockEnd = position + (numSamples - sample) * beatsPerSample;
            const double target = nextEvent < events.size() ? events[nextEvent].beat : lengthBeats;

            if (target >= blockEnd)
            {
                position = blockEnd;
                return;
            }

            const int at = juce::jlimit (sample, numSamples - 1, sample + (int) ((target - position) / beatsPerSample));
            position = target;
            sample = at;

            if (nextEvent < events.size())
            {
                emit (midi, events[nextEvent].message, at);
                ++nextEvent;
                continue;
            }

            // Fin du fichier : on reboucle, ou on s'arrête au début.
            releaseNotes (midi, at);
            seek (0.0);
            if (! looping || ++wraps > 2)
            {
                playing = looping.load();
                return;
            }
        }
    }

private:
    struct Event
    {
        double beat;
        juce::MidiMessage message;
    };

    void seek (double beat) noexcept
    {
        position = beat;
        nextEvent = 0;
        while (nextEvent < events.size() && events[nextEvent].beat < beat)
            ++nextEvent;
    }

    void emit (juce::MidiBuffer& midi, const juce::MidiMessage& m, int sample)
    {
        if (m.isNoteOn())
            held[(size_t) m.getNoteNumber()] = true;
        else if (m.isNoteOff())
            held[(size_t) m.getNoteNumber()] = false;
        midi.addEvent (m, sample);
    }

    // Relâche les notes du fichier encore tenues (pause, retour au début, fin).
    void releaseNotes (juce::MidiBuffer& midi, int sample)
    {
        for (int n = 0; n < 128; ++n)
            if (held[(size_t) n])
                midi.addEvent (juce::MidiMessage::noteOff (1, n), sample);
        held.fill (false);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), sample);   // pédale relevée
    }

    juce::SpinLock eventsLock;
    std::vector<Event> events;
    double sampleRate = 0.0, lengthBeats = 4.0, position = 0.0;
    size_t nextEvent = 0;
    std::array<bool, 128> held {};

    std::atomic<bool> playing { false }, looping { true }, loadedFlag { false };
    std::atomic<bool> flushRequested { false }, restartRequested { false };
    std::atomic<double> tempo { 120.0 };
    juce::String fileName;
};

//==============================================================================
// Panneau du lecteur, affiché à gauche du clavier quand un fichier est chargé.
class MidiPlayerPanel final : public juce::Component,
                              private juce::Timer
{
public:
    MidiPlayerPanel (MidiFilePlayer& p, juce::Colour accentColour, juce::Colour textColour)
        : player (p), accent (accentColour), text (textColour)
    {
        playButton.onClick = [this] { player.setPlaying (! player.isPlaying()); refresh(); };
        restartButton.onClick = [this] { player.restart(); };
        loopButton.setClickingTogglesState (true);
        loopButton.onClick = [this] { player.setLooping (loopButton.getToggleState()); };
        closeButton.onClick = [this] { player.unload(); if (onClose != nullptr) onClose(); };

        for (auto* b : { &playButton, &restartButton, &loopButton, &closeButton })
        {
            b->setColour (juce::TextButton::buttonOnColourId, accent);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
            addAndMakeVisible (b);
        }
        restartButton.setTooltip (juce::String::fromUTF8 ("Revenir au d\xc3\xa9" "but"));
        loopButton.setTooltip ("Boucle");
        closeButton.setTooltip ("Retirer le fichier");

        tempoSlider.setSliderStyle (juce::Slider::IncDecButtons);
        tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 70, 20);
        tempoSlider.setRange (40.0, 240.0, 1.0);
        tempoSlider.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
        tempoSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " BPM"; };
        tempoSlider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
        tempoSlider.setColour (juce::Slider::textBoxTextColourId, text);
        tempoSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0a0a0a));
        tempoSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff444444));
        tempoSlider.onValueChange = [this] { player.setTempo (tempoSlider.getValue()); };
        addAndMakeVisible (tempoSlider);

        startTimerHz (10);
    }

    std::function<void()> onClose;

    // Rafraîchit le panneau après le chargement d'un fichier.
    void refresh()
    {
        playButton.setButtonText (player.isPlaying() ? juce::String::fromUTF8 ("\xe2\x9d\x9a\xe2\x9d\x9a")
                                                     : juce::String::fromUTF8 ("\xe2\x96\xb6"));
        playButton.setToggleState (player.isPlaying(), juce::dontSendNotification);
        loopButton.setToggleState (player.isLooping(), juce::dontSendNotification);
        tempoSlider.setValue (player.getTempo(), juce::dontSendNotification);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xff101010));
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (accent.withAlpha (0.6f));
        g.drawRoundedRectangle (area, 4.0f, 1.0f);

        g.setColour (text);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawFittedText (player.getFileName(), titleArea, juce::Justification::centredLeft, 1);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (6, 5);
        auto top = area.removeFromTop (18);
        closeButton.setBounds (top.removeFromRight (18));
        titleArea = top.withTrimmedRight (4);

        area.removeFromTop (4);
        auto buttons = area.removeFromTop (24);
        const int w = buttons.getWidth() / 3;
        restartButton.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
        playButton.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
        loopButton.setBounds (buttons.reduced (2, 0));

        area.removeFromTop (4);
        tempoSlider.setBounds (area.removeFromTop (22));
    }

private:
    void timerCallback() override
    {
        // La lecture s'arrête seule en fin de fichier sans boucle.
        if (playButton.getToggleState() != player.isPlaying())
            refresh();
    }

    MidiFilePlayer& player;
    juce::Colour accent, text;
    juce::TextButton playButton, restartButton { juce::String::fromUTF8 ("\xe2\x8f\xae") },
                     loopButton { juce::String::fromUTF8 ("\xe2\x9f\xb3") }, closeButton { juce::String::fromUTF8 ("\xc3\x97") };
    juce::Slider tempoSlider;
    juce::Rectangle<int> titleArea;
};
