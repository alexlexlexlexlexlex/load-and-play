// Le moteur : carte son, claviers MIDI, instruments virtuels et ports MIDI de
// sortie, autour du séquenceur (Session). Le séquenceur et les sorties sont
// protégés par un verrou : l'interface les modifie avec edit() / read().
#pragma once

#include "Session.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <map>

#include <ableton/Link.hpp>

namespace gridplay
{
class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::MidiInputCallback,
                    private juce::Timer
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void initialise (const juce::XmlElement* savedAudioState);

    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    //==========================================================================
    // Accès au séquenceur sous verrou. edit() marque le morceau comme modifié.
    template <typename Fn>
    auto edit (Fn&& fn)
    {
        const juce::ScopedLock sl (lock);
        ++changeCount;
        return fn (session);
    }

    template <typename Fn>
    auto read (Fn&& fn) const
    {
        const juce::ScopedLock sl (lock);
        return fn (session);
    }

    // Lancer, arrêter : ne compte pas comme une modification du morceau.
    template <typename Fn>
    auto perform (Fn&& fn)
    {
        const juce::ScopedLock sl (lock);
        return fn (session);
    }

    int getChangeCount() const { return changeCount.load(); }

    //==========================================================================
    // Pistes (avec leurs sorties).
    void insertTrack (int index);
    void removeTrack (int index);
    void setSong (Song song);

    // Sorties d'une piste.
    void setTrackPlugin (int track, const juce::PluginDescription& description, juce::MemoryBlock state = {}, bool userChange = true);
    void setTrackMidiPort (int track, const juce::String& portName, int channel, bool userChange = true);
    void setTrackOutputChannel (int track, int channel);
    void clearTrackOutput (int track);
    juce::AudioPluginInstance* getPlugin (int track);
    void storePluginStates();       // avant la sauvegarde

    //==========================================================================
    // Inserts d'effets d'une voie (piste : son index ; master : -1), jusqu'à 8.
    static constexpr int masterTarget = -1;
    void addInsert (int target, int slot, const juce::PluginDescription& description, juce::MemoryBlock state = {}, bool userChange = true);
    void removeInsert (int target, int slot);
    void moveInsert (int target, int from, int to);
    juce::AudioPluginInstance* getInsert (int target, int slot);

    // Retour audio des synthés externes : noms des entrées de la carte son.
    juce::StringArray getAudioInputNames() const;
    void ensureAudioInputs (int channelsNeeded);

    // Niveaux des vu-mètres (crêtes qui retombent) : un par piste, puis le master.
    std::vector<float> getMeters() const;

    // Appelé avant qu'un instrument disparaisse (pour fermer sa fenêtre).
    std::function<void (juce::AudioPluginInstance*)> onPluginRemoving;
    std::function<void (const juce::String& message)> onError;

    //==========================================================================
    // Appareils MIDI.
    juce::StringArray getInputNames() const;
    juce::StringArray getOutputNames() const;
    static juce::String virtualPortName();
    bool hasVirtualPort() const { return virtualPort != nullptr; }

    // Écoute d'une note depuis l'éditeur (vélocité 0 = relâcher), même si la piste n'est pas armée.
    void previewNote (int track, int pitch, int velocity);

    // L'APC mini : ses messages ne vont pas aux pistes mais à ce gestionnaire.
    void setController (const juce::String& inputIdentifier, std::function<void (const juce::MidiMessage&)> handler);

    double getSampleRate() const { return sampleRate; }

    //==========================================================================
    // Ableton Link : tempo et mesure partagés avec Serato DJ, Ableton Live, Traktor...
    // (sur cet ordinateur ou le réseau local). Le tempo se règle alors pour tout le monde.
    void setLinkEnabled (bool enabled);
    bool isLinkEnabled() const;
    int getLinkPeers() const;
    void setTempo (double bpm);
    // Pour les tests : phase de Grid Play et phase de Link dans la mesure, au même instant.
    std::pair<double, double> getLinkPhases();

    // Crête de la sortie depuis la dernière lecture (pour un futur vu-mètre, et les tests).
    float takeOutputPeak() { return outputPeak.exchange (0.0f); }

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;
    void timerCallback() override;

    void refreshMidiInputs();
    juce::MidiOutput* openPort (const juce::String& name);
    int indexOfTrack (int trackId) const;
    void sendNotesOff (juce::MidiOutput* port, int channel, const juce::MidiBuffer& offs);
    void updateExcludedInputs();

    struct TrackOutput
    {
        std::unique_ptr<juce::AudioPluginInstance> plugin;
        juce::MidiOutput* port = nullptr;
        int channel = 1;
        int lastVolumeCC = -1;
        EqProcessor eq;
        std::vector<std::unique_ptr<juce::AudioPluginInstance>> inserts;   // alignés sur Track::inserts
        float peak = 0.0f;
    };

    std::vector<InsertSlot>* modelInserts (int target);
    std::vector<std::unique_ptr<juce::AudioPluginInstance>>* instanceInserts (int target);
    void runInsert (juce::AudioPluginInstance& plugin, juce::AudioBuffer<float>& audio, int numSamples);
    void prepareInstance (juce::AudioPluginInstance& plugin);

    juce::CriticalSection lock;
    Session session;
    std::vector<std::unique_ptr<TrackOutput>> outputs;   // une par piste, sous le verrou
    EqProcessor masterEqProcessor;
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> masterInsertInstances;
    juce::AudioBuffer<float> trackBuffer, masterBuffer;
    juce::MidiBuffer emptyMidi;
    float masterPeak = 0.0f;
    std::atomic<int> changeCount { 0 };
    std::atomic<float> outputPeak { 0.0f };
    std::unique_ptr<ableton::Link> link;
    double outputLatencyMicros = 0.0;
    bool linkWasPlaying = false;
    void syncToLink (int numSamples);

    std::map<juce::String, std::unique_ptr<juce::MidiOutput>> openPorts;
    std::unique_ptr<juce::MidiOutput> virtualPort;

    // Messages des claviers : file sans verrou entre le fil MIDI et le fil audio.
    struct InputEvent
    {
        int deviceHash = 0;
        juce::MidiMessage message;
    };
    juce::AbstractFifo fifo { 2048 };
    std::vector<InputEvent> fifoData = std::vector<InputEvent> (2048);

    juce::AbstractFifo previewFifo { 256 };
    std::vector<LiveEvent> previewData = std::vector<LiveEvent> (256);

    std::vector<LiveEvent> live;
    std::vector<juce::MidiBuffer> trackMidi;
    std::vector<std::pair<juce::MidiOutput*, juce::MidiBuffer>> portBuffers;
    juce::AudioBuffer<float> pluginBuffer;
    double sampleRate = 44100.0;
    int blockSize = 512;

    juce::CriticalSection controllerLock;
    juce::String controllerId;
    std::function<void (const juce::MidiMessage&)> controllerHandler;
    juce::Array<int> excludedInputs;     // nos propres sorties : jamais relues en entrée

    juce::StringArray enabledInputs;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
} // namespace gridplay
