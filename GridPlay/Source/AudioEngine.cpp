#include "AudioEngine.h"

namespace gridplay
{
AudioEngine::AudioEngine()
{
    formatManager.addDefaultFormats();
    live.reserve (2048);
    trackMidi.resize ((size_t) session.song.numTracks());
    outputs.resize ((size_t) session.song.numTracks());
    for (auto& o : outputs)
        o = std::make_unique<TrackOutput>();
}

AudioEngine::~AudioEngine()
{
    *alive = false;
    stopTimer();
    deviceManager.removeMidiInputDeviceCallback ({}, this);
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();

    // Plus de son : on coupe les notes encore tenues sur les ports MIDI.
    const juce::ScopedLock sl (lock);
    for (int t = 0; t < session.song.numTracks(); ++t)
        if (auto* port = outputs[(size_t) t]->port)
            sendNotesOff (port, outputs[(size_t) t]->channel, session.takeAllNotesOff (t));
}

juce::String AudioEngine::virtualPortName() { return "Grid Play"; }

void AudioEngine::initialise (const juce::XmlElement* savedAudioState)
{
    const auto error = deviceManager.initialise (0, 2, savedAudioState, true);
    if (error.isNotEmpty())
        DBG ("Carte son : " << error);

   #if ! JUCE_WINDOWS
    // Port MIDI virtuel « Grid Play » : un DAW peut le choisir comme entrée.
    virtualPort = juce::MidiOutput::createNewDevice (virtualPortName());
    if (virtualPort != nullptr)
        virtualPort->startBackgroundThread();
   #endif

    deviceManager.addMidiInputDeviceCallback ({}, this);
    deviceManager.addAudioCallback (this);
    refreshMidiInputs();
    startTimer (1000);
}

//==============================================================================
// Appareils MIDI
void AudioEngine::refreshMidiInputs()
{
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (device.name == virtualPortName() || enabledInputs.contains (device.identifier))
            continue;
        deviceManager.setMidiInputDeviceEnabled (device.identifier, true);
        enabledInputs.add (device.identifier);
    }
}

void AudioEngine::timerCallback()
{
    refreshMidiInputs();
}

juce::StringArray AudioEngine::getInputNames() const
{
    juce::StringArray names;
    for (const auto& device : juce::MidiInput::getAvailableDevices())
        if (device.name != virtualPortName())
            names.addIfNotAlreadyThere (device.name);
    return names;
}

juce::StringArray AudioEngine::getOutputNames() const
{
    juce::StringArray names;
    if (virtualPort != nullptr)
        names.add (virtualPortName());
    for (const auto& device : juce::MidiOutput::getAvailableDevices())
        if (device.name != virtualPortName())
            names.addIfNotAlreadyThere (device.name);
    return names;
}

juce::MidiOutput* AudioEngine::openPort (const juce::String& name)
{
    if (name == virtualPortName())
        return virtualPort.get();

    auto it = openPorts.find (name);
    if (it != openPorts.end() && it->second != nullptr)
        return it->second.get();

    for (const auto& device : juce::MidiOutput::getAvailableDevices())
        if (device.name == name)
            if (auto out = juce::MidiOutput::openDevice (device.identifier))
            {
                out->startBackgroundThread();
                auto* raw = out.get();
                openPorts[name] = std::move (out);
                return raw;
            }
    return nullptr;
}

void AudioEngine::previewNote (int track, int pitch, int velocity)
{
    const auto message = velocity > 0 ? juce::MidiMessage::noteOn (1, juce::jlimit (0, 127, pitch), (juce::uint8) juce::jlimit (1, 127, velocity))
                                      : juce::MidiMessage::noteOff (1, juce::jlimit (0, 127, pitch));
    const auto scope = previewFifo.write (1);
    if (scope.blockSize1 > 0)
        previewData[(size_t) scope.startIndex1] = { track, message };
}

void AudioEngine::setController (const juce::String& inputIdentifier, std::function<void (const juce::MidiMessage&)> handler)
{
    const juce::ScopedLock sl (controllerLock);
    controllerId = inputIdentifier;
    controllerHandler = std::move (handler);
}

void AudioEngine::updateExcludedInputs()
{
    juce::Array<int> hashes;
    {
        const juce::ScopedLock sl (lock);
        for (const auto& t : session.song.tracks)
            if (t.outputType == OutputType::midiPort)
                hashes.addIfNotAlreadyThere (t.outputPort.hashCode());
    }
    const juce::ScopedLock sl (controllerLock);
    excludedInputs = hashes;
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message)
{
    if (source == nullptr || message.isSysEx() || message.isMidiClock() || message.isActiveSense()
        || message.isSongPositionPointer() || message.isMidiStart() || message.isMidiStop() || message.isMidiContinue())
        return;

    const int hash = source->getName().hashCode();
    {
        const juce::ScopedLock sl (controllerLock);
        if (source->getIdentifier() == controllerId)
        {
            if (controllerHandler)
                controllerHandler (message);
            return;
        }
        // Un port que nous utilisons en sortie, relu en entrée (IAC, loopMIDI...) : boucle infinie.
        if (excludedInputs.contains (hash))
            return;
    }

    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0)
        fifoData[(size_t) scope.startIndex1] = { hash, message };
}

//==============================================================================
// Son
void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const juce::ScopedLock sl (lock);
    sampleRate = device->getCurrentSampleRate();
    blockSize = device->getCurrentBufferSizeSamples();
    session.prepare (sampleRate);
    pluginBuffer.setSize (16, blockSize);
    for (auto& o : outputs)
        if (o->plugin != nullptr)
        {
            o->plugin->setRateAndBufferSizeDetails (sampleRate, blockSize);
            o->plugin->prepareToPlay (sampleRate, blockSize);
        }
}

void AudioEngine::audioDeviceStopped()
{
    const juce::ScopedLock sl (lock);
    for (auto& o : outputs)
        if (o->plugin != nullptr)
            o->plugin->releaseResources();
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                    float* const* outputChannelData, int numOutputChannels,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = 0; c < numOutputChannels; ++c)
        if (outputChannelData[c] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[c], numSamples);

    const juce::ScopedLock sl (lock);
    auto& song = session.song;

    // Claviers -> pistes armées (les relâchements vont partout : une note ne reste jamais coincée).
    live.clear();
    const auto scope = fifo.read (fifo.getNumReady());
    auto route = [&] (int start, int size)
    {
        for (int i = start; i < start + size; ++i)
        {
            const auto& ev = fifoData[(size_t) i];
            for (int t = 0; t < song.numTracks(); ++t)
            {
                const auto& tr = song.tracks[(size_t) t];
                if (! tr.inputEnabled || (! tr.arm && ! ev.message.isNoteOff()))
                    continue;
                if (tr.inputDevice.isNotEmpty() && tr.inputDevice.hashCode() != ev.deviceHash)
                    continue;
                if (tr.inputChannel > 0 && ev.message.getChannel() != tr.inputChannel)
                    continue;
                if (live.size() < live.capacity())
                    live.push_back ({ t, ev.message });
            }
        }
    };
    route (scope.startIndex1, scope.blockSize1);
    route (scope.startIndex2, scope.blockSize2);

    // Notes écoutées depuis l'éditeur.
    const auto preview = previewFifo.read (previewFifo.getNumReady());
    for (int i = 0; i < preview.blockSize1 + preview.blockSize2; ++i)
    {
        const int index = i < preview.blockSize1 ? preview.startIndex1 + i : preview.startIndex2 + i - preview.blockSize1;
        if (live.size() < live.capacity() && juce::isPositiveAndBelow (previewData[(size_t) index].track, song.numTracks()))
            live.push_back (previewData[(size_t) index]);
    }

    session.process (numSamples, live, trackMidi);

    for (auto& p : portBuffers)
        p.second.clear();

    const float master = volumeToGain (song.masterVolume);
    for (int t = 0; t < song.numTracks() && t < (int) outputs.size(); ++t)
    {
        auto& out = *outputs[(size_t) t];
        auto& midi = trackMidi[(size_t) t];
        const auto& tr = song.tracks[(size_t) t];

        if (out.plugin != nullptr)
        {
            auto& plugin = *out.plugin;
            const int channels = juce::jmax (2, plugin.getTotalNumInputChannels(), plugin.getTotalNumOutputChannels());
            if (channels > pluginBuffer.getNumChannels() || numSamples > pluginBuffer.getNumSamples())
                continue;   // ne devrait pas arriver : taille préparée au démarrage
            juce::AudioBuffer<float> buffer (pluginBuffer.getArrayOfWritePointers(), channels, numSamples);
            buffer.clear();
            plugin.processBlock (buffer, midi);

            const float gain = volumeToGain (tr.volume) * master;
            const int pluginOuts = juce::jmax (1, plugin.getTotalNumOutputChannels());
            for (int c = 0; c < juce::jmin (2, numOutputChannels); ++c)
                if (outputChannelData[c] != nullptr)
                    juce::FloatVectorOperations::addWithMultiply (outputChannelData[c], buffer.getReadPointer (juce::jmin (c, pluginOuts - 1)),
                                                                  gain, numSamples);
        }
        else if (out.port != nullptr)
        {
            auto it = std::find_if (portBuffers.begin(), portBuffers.end(), [&] (const auto& p) { return p.first == out.port; });
            if (it == portBuffers.end())
                continue;   // ajouté par le fil principal à la connexion
            // Volume de la piste en CC 7, envoyé quand il change.
            const int volumeCC = juce::jlimit (0, 127, juce::roundToInt (tr.volume * 127.0f));
            if (volumeCC != out.lastVolumeCC)
            {
                it->second.addEvent (juce::MidiMessage::controllerEvent (out.channel, 7, volumeCC), 0);
                out.lastVolumeCC = volumeCC;
            }
            for (const auto meta : midi)
            {
                auto m = meta.getMessage();
                if (m.getChannel() > 0)
                    m.setChannel (out.channel);
                it->second.addEvent (m, meta.samplePosition);
            }
        }
    }

    const double now = juce::Time::getMillisecondCounterHiRes();
    for (auto& p : portBuffers)
        if (! p.second.isEmpty())
            p.first->sendBlockOfMessages (p.second, now, sampleRate);

    float peak = outputPeak.load();
    for (int c = 0; c < juce::jmin (2, numOutputChannels); ++c)
        if (outputChannelData[c] != nullptr)
            peak = juce::jmax (peak, juce::FloatVectorOperations::findMaximum (outputChannelData[c], numSamples),
                               -juce::FloatVectorOperations::findMinimum (outputChannelData[c], numSamples));
    outputPeak = peak;

    for (int c = 2; c < numOutputChannels; ++c)
        if (outputChannelData[c] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[c], numSamples);
}

//==============================================================================
// Pistes et sorties
int AudioEngine::indexOfTrack (int trackId) const
{
    for (int t = 0; t < session.song.numTracks(); ++t)
        if (session.song.tracks[(size_t) t].id == trackId)
            return t;
    return -1;
}

void AudioEngine::sendNotesOff (juce::MidiOutput* port, int channel, const juce::MidiBuffer& offs)
{
    if (port == nullptr)
        return;
    for (const auto meta : offs)
    {
        auto m = meta.getMessage();
        m.setChannel (channel);
        port->sendMessageNow (m);
    }
}

void AudioEngine::insertTrack (int index)
{
    {
        const juce::ScopedLock sl (lock);
        index = juce::jlimit (0, session.song.numTracks(), index);
        session.insertTrack (index, makeTrack (session.song.numTracks(), session.song.numScenes()));
        outputs.insert (outputs.begin() + index, std::make_unique<TrackOutput>());
        trackMidi.resize (outputs.size());
        ++changeCount;
    }
}

void AudioEngine::removeTrack (int index)
{
    std::unique_ptr<TrackOutput> removed;
    juce::MidiBuffer offs;
    {
        const juce::ScopedLock sl (lock);
        if (! juce::isPositiveAndBelow (index, session.song.numTracks()))
            return;
        offs = session.takeAllNotesOff (index);
        removed = std::move (outputs[(size_t) index]);
        outputs.erase (outputs.begin() + index);
        session.removeTrack (index);
        trackMidi.resize (outputs.size());
        ++changeCount;
    }
    if (removed->port != nullptr)
        sendNotesOff (removed->port, removed->channel, offs);
    if (removed->plugin != nullptr && onPluginRemoving)
        onPluginRemoving (removed->plugin.get());
    updateExcludedInputs();
}

void AudioEngine::setSong (Song song)
{
    std::vector<std::unique_ptr<TrackOutput>> old;
    {
        const juce::ScopedLock sl (lock);
        for (int t = 0; t < session.song.numTracks(); ++t)
            if (auto* port = outputs[(size_t) t]->port)
                sendNotesOff (port, outputs[(size_t) t]->channel, session.takeAllNotesOff (t));
        old = std::move (outputs);
        outputs.clear();
        session.setSong (std::move (song));
        session.prepare (sampleRate);
        for (int t = 0; t < session.song.numTracks(); ++t)
            outputs.push_back (std::make_unique<TrackOutput>());
        trackMidi.resize (outputs.size());
        changeCount = 0;
    }
    for (auto& o : old)
        if (o->plugin != nullptr && onPluginRemoving)
            onPluginRemoving (o->plugin.get());
    old.clear();

    // Recharge les instruments et rouvre les ports du morceau.
    Song copy = read ([] (const Session& s) { return s.song; });
    for (int t = 0; t < copy.numTracks(); ++t)
    {
        const auto& tr = copy.tracks[(size_t) t];
        if (tr.outputType == OutputType::plugin)
        {
            juce::PluginDescription description;
            if (auto xml = juce::parseXML (tr.pluginDescription); xml != nullptr && description.loadFromXml (*xml))
                setTrackPlugin (t, description, tr.pluginState, false);
            else if (onError)
                onError ("Instrument introuvable : " + tr.pluginName);
        }
        else if (tr.outputType == OutputType::midiPort)
        {
            setTrackMidiPort (t, tr.outputPort, tr.outputChannel, false);
        }
    }
    changeCount = 0;
}

void AudioEngine::setTrackPlugin (int track, const juce::PluginDescription& description, juce::MemoryBlock state, bool userChange)
{
    const int trackId = read ([track] (const Session& s)
                              { return juce::isPositiveAndBelow (track, s.song.numTracks()) ? s.song.tracks[(size_t) track].id : -1; });
    if (trackId < 0)
        return;

    auto safe = alive;
    formatManager.createPluginInstanceAsync (description, sampleRate, blockSize,
        [this, safe, trackId, description, state, userChange] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
        {
            if (! *safe)
                return;
            if (instance == nullptr)
            {
                if (onError)
                    onError ("Impossible de charger " + description.name + (error.isNotEmpty() ? " : " + error : juce::String()));
                return;
            }

            instance->enableAllBuses();
            instance->setRateAndBufferSizeDetails (sampleRate, blockSize);
            instance->prepareToPlay (sampleRate, blockSize);
            if (state.getSize() > 0)
                instance->setStateInformation (state.getData(), (int) state.getSize());

            std::unique_ptr<juce::AudioPluginInstance> old;
            juce::MidiOutput* oldPort = nullptr;
            int oldChannel = 1;
            juce::MidiBuffer offs;
            {
                const juce::ScopedLock sl (lock);
                const int t = indexOfTrack (trackId);
                if (t < 0)
                    return;
                const int needed = juce::jmax (2, instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels());
                if (needed > pluginBuffer.getNumChannels())
                    pluginBuffer.setSize (needed, juce::jmax (blockSize, pluginBuffer.getNumSamples()));
                auto& out = *outputs[(size_t) t];
                offs = session.takeAllNotesOff (t);
                old = std::move (out.plugin);
                oldPort = out.port;
                oldChannel = out.channel;
                out.plugin = std::move (instance);
                out.port = nullptr;
                auto& tr = session.song.tracks[(size_t) t];
                tr.outputType = OutputType::plugin;
                tr.pluginName = description.name;
                if (auto xml = description.createXml())
                    tr.pluginDescription = xml->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader());
                if (userChange)
                    ++changeCount;
            }
            sendNotesOff (oldPort, oldChannel, offs);
            if (old != nullptr && onPluginRemoving)
                onPluginRemoving (old.get());
            updateExcludedInputs();
        });
}

void AudioEngine::setTrackMidiPort (int track, const juce::String& portName, int channel, bool userChange)
{
    auto* port = openPort (portName);
    if (port == nullptr && onError)
        onError ("Port MIDI introuvable : " + portName);

    std::unique_ptr<juce::AudioPluginInstance> old;
    juce::MidiOutput* oldPort = nullptr;
    int oldChannel = 1;
    juce::MidiBuffer offs;
    {
        const juce::ScopedLock sl (lock);
        if (! juce::isPositiveAndBelow (track, session.song.numTracks()))
            return;
        auto& out = *outputs[(size_t) track];
        offs = session.takeAllNotesOff (track);
        old = std::move (out.plugin);
        oldPort = out.port;
        oldChannel = out.channel;
        out.port = port;
        out.channel = juce::jlimit (1, 16, channel);
        out.lastVolumeCC = -1;
        if (port != nullptr && std::none_of (portBuffers.begin(), portBuffers.end(), [port] (const auto& p) { return p.first == port; }))
        {
            portBuffers.emplace_back (port, juce::MidiBuffer());
            portBuffers.back().second.ensureSize (4096);
        }
        auto& tr = session.song.tracks[(size_t) track];
        tr.outputType = OutputType::midiPort;
        tr.outputPort = portName;
        tr.outputChannel = out.channel;
        if (userChange)
            ++changeCount;
    }
    sendNotesOff (oldPort, oldChannel, offs);
    if (old != nullptr && onPluginRemoving)
        onPluginRemoving (old.get());
    updateExcludedInputs();
}

void AudioEngine::setTrackOutputChannel (int track, int channel)
{
    juce::MidiOutput* port = nullptr;
    int oldChannel = 1;
    juce::MidiBuffer offs;
    {
        const juce::ScopedLock sl (lock);
        if (! juce::isPositiveAndBelow (track, session.song.numTracks()))
            return;
        auto& out = *outputs[(size_t) track];
        port = out.port;
        oldChannel = out.channel;
        offs = session.takeAllNotesOff (track);
        out.channel = juce::jlimit (1, 16, channel);
        out.lastVolumeCC = -1;
        session.song.tracks[(size_t) track].outputChannel = out.channel;
        ++changeCount;
    }
    sendNotesOff (port, oldChannel, offs);
}

void AudioEngine::clearTrackOutput (int track)
{
    std::unique_ptr<juce::AudioPluginInstance> old;
    juce::MidiOutput* oldPort = nullptr;
    int oldChannel = 1;
    juce::MidiBuffer offs;
    {
        const juce::ScopedLock sl (lock);
        if (! juce::isPositiveAndBelow (track, session.song.numTracks()))
            return;
        auto& out = *outputs[(size_t) track];
        offs = session.takeAllNotesOff (track);
        old = std::move (out.plugin);
        oldPort = out.port;
        oldChannel = out.channel;
        out.port = nullptr;
        auto& tr = session.song.tracks[(size_t) track];
        tr.outputType = OutputType::none;
        tr.pluginName = {};
        tr.pluginDescription = {};
        tr.pluginState.reset();
        ++changeCount;
    }
    sendNotesOff (oldPort, oldChannel, offs);
    if (old != nullptr && onPluginRemoving)
        onPluginRemoving (old.get());
    updateExcludedInputs();
}

juce::AudioPluginInstance* AudioEngine::getPlugin (int track)
{
    const juce::ScopedLock sl (lock);
    return juce::isPositiveAndBelow (track, (int) outputs.size()) ? outputs[(size_t) track]->plugin.get() : nullptr;
}

void AudioEngine::storePluginStates()
{
    const int count = read ([] (const Session& s) { return s.song.numTracks(); });
    for (int t = 0; t < count; ++t)
        if (auto* plugin = getPlugin (t))
        {
            juce::MemoryBlock state;
            plugin->getStateInformation (state);
            const juce::ScopedLock sl (lock);
            if (juce::isPositiveAndBelow (t, session.song.numTracks()))
                session.song.tracks[(size_t) t].pluginState = state;
        }
}
} // namespace gridplay
