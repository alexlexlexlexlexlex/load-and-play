// Le séquenceur : transport, lancement des clips calé sur la grille, boucles,
// enregistrement des claviers. Il ne connaît ni la carte son ni les ports MIDI :
// à chaque bloc, il rend un MidiBuffer par piste (canal 1), au sample près.
//
// Toutes les méthodes s'appellent sous le verrou de l'AudioEngine.
#pragma once

#include "Model.h"

namespace gridplay
{
enum class SlotState { empty, stopped, playQueued, playing, recordQueued, recording };

// Un message reçu d'un clavier, déjà attribué à une piste.
struct LiveEvent
{
    int track = 0;
    juce::MidiMessage message;
};

class Session
{
public:
    Session();

    Song song;

    void prepare (double sampleRate);

    //==========================================================================
    // Commandes (lancement calé sur la quantification du morceau).
    void launchSlot (int track, int scene);
    void stopTrack (int track);
    void launchScene (int scene);
    void stopAllClips();
    void play();
    void stop();
    bool isPlaying() const          { return playing; }
    double getBeat() const          { return beat; }

    //==========================================================================
    // État, pour l'écran et les LED de l'APC mini.
    SlotState getSlotState (int track, int scene) const;
    int getPlayingSlot (int track) const;
    double getClipProgress (int track) const;    // 0..1 dans la boucle du clip qui joue
    bool isTrackActive (int track) const;         // un clip joue ou s'enregistre
    bool isSceneActive (int scene) const;

    //==========================================================================
    // Structure : ces méthodes gardent l'état de lecture aligné sur le morceau.
    void setSong (Song newSong);
    void insertTrack (int index, Track track);
    void removeTrack (int index);
    void insertScene (int index, Scene scene);
    void removeScene (int index);
    void setClip (int track, int scene, ClipPtr clip);
    void moveClip (int fromTrack, int fromScene, int toTrack, int toScene, bool copy);

    // Oublie les notes en cours d'une piste (sa sortie vient de changer).
    void forgetActiveNotes (int track);
    // Les notes à couper sur une piste, pour la sortie qui s'en va.
    juce::MidiBuffer takeAllNotesOff (int track);

    //==========================================================================
    // Rend un bloc : outputs[t] reçoit les messages de la piste t (canal 1).
    void process (int numSamples, const std::vector<LiveEvent>& live, std::vector<juce::MidiBuffer>& outputs);

private:
    enum class Queued { none, play, stop, record };

    struct ActiveNote
    {
        int pitch;
        double offBeat;
    };

    struct Runtime
    {
        int playingSlot = -1;
        double clipStart = 0.0;           // temps où la boucle a commencé
        Queued queued = Queued::none;
        int queuedSlot = -1;
        double queuedBeat = -1.0;         // calculé par le premier bloc qui le voit

        bool recording = false;
        int recordSlot = -1;
        double recordStart = 0.0;
        ClipPtr recordClip;
        std::array<double, 128> heldStart {};   // début des notes tenues pendant l'enregistrement
        std::array<juce::uint8, 128> heldVelocity {};

        std::vector<ActiveNote> active;          // notes des clips en cours
        std::array<bool, 128> liveHeld {};       // notes jouées au clavier, en cours
        bool wasMuted = false;
    };

    std::vector<Runtime> runtime;
    double sampleRate = 44100.0;
    bool playing = false;
    double beat = 0.0;

    double boundaryAfter (double from) const;
    void queue (int track, Queued what, int slot);
    void playRange (int track, double from, double to, double blockStart, double samplesPerBeat, int numSamples, juce::MidiBuffer& out, bool muted);
    void applyQueued (int track, double at, int sample, juce::MidiBuffer& out);
    void allNotesOff (Runtime& rt, int sample, juce::MidiBuffer& out);
    void finishRecording (int track, double at);
    void startRecording (int track, int slot, double at);
    bool anySolo() const;
    void resizeRuntime();
};
} // namespace gridplay
