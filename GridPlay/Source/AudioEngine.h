// Le moteur : carte son, claviers MIDI, instruments virtuels et ports MIDI de
// sortie, autour du séquenceur (Session). Le séquenceur et les sorties sont
// protégés par un verrou : l'interface les modifie avec edit() / read().
#pragma once

#include "Session.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <map>

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

    // Appelé avant qu'un instrument disparaisse (pour fermer sa fenêtre).
    std::function<void (juce::AudioPluginInstance*)> onPluginRemoving;
    std::function<void (const juce::String& message)> onError;

    //==========================================================================
    // Appareils MIDI.
    juce::StringArray getInputNames() const;
    juce::StringArray getOutputNames() const;
    static juce::String virtualPortName();
    bool hasVirtualPort() const { return virtualPort != nullptr; }

    // L'APC mini : ses messages ne vont pas aux pistes mais à ce gestionnaire.
    void setController (const juce::String& inputIdentifier, std::function<void (const juce::MidiMessage&)> handler);

    double getSampleRate() const { return sampleRate; }

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
    };

    juce::CriticalSection lock;
    Session session;
    std::vector<std::unique_ptr<TrackOutput>> outputs;   // une par piste, sous le verrou
    std::atomic<int> changeCount { 0 };
    std::atomic<float> outputPeak { 0.0f };

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
