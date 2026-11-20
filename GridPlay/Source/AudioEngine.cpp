#include "AudioEngine.h"

namespace gridplay
{
AudioEngine::AudioEngine()
{
    link = std::make_unique<ableton::Link> (120.0);
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
    outputLatencyMicros = (device->getOutputLatencyInSamples() + blockSize) / sampleRate * 1.0e6;
    session.prepare (sampleRate);
    const int capacity = juce::jmax (blockSize, 2048);
    pluginBuffer.setSize (juce::jmax (16, pluginBuffer.getNumChannels()), capacity);
    trackBuffer.setSize (2, capacity);
    masterBuffer.setSize (2, capacity);
    masterEqProcessor.prepare (sampleRate);
    for (auto& o : outputs)
    {
        o->eq.prepare (sampleRate);
        if (o->plugin != nullptr)
            prepareInstance (*o->plugin);
        for (auto& insert : o->inserts)
            if (insert != nullptr)
                prepareInstance (*insert);
    }
    for (auto& insert : masterInsertInstances)
        if (insert != nullptr)
            prepareInstance (*insert);
}

void AudioEngine::prepareInstance (juce::AudioPluginInstance& plugin)
{
    plugin.setRateAndBufferSizeDetails (sampleRate, blockSize);
    plugin.prepareToPlay (sampleRate, blockSize);
}

void AudioEngine::audioDeviceStopped()
{
    const juce::ScopedLock sl (lock);
    for (auto& o : outputs)
    {
        if (o->plugin != nullptr)
            o->plugin->releaseResources();
        for (auto& insert : o->inserts)
            if (insert != nullptr)
                insert->releaseResources();
    }
    for (auto& insert : masterInsertInstances)
        if (insert != nullptr)
            insert->releaseResources();
}

void AudioEngine::runInsert (juce::AudioPluginInstance& plugin, juce::AudioBuffer<float>& audio, int numSamples)
{
    const int channels = juce::jmax (2, plugin.getTotalNumInputChannels(), plugin.getTotalNumOutputChannels());
    if (channels > pluginBuffer.getNumChannels() || numSamples > pluginBuffer.getNumSamples())
        return;
    juce::AudioBuffer<float> work (pluginBuffer.getArrayOfWritePointers(), channels, numSamples);
    work.clear();
    for (int c = 0; c < 2; ++c)
        work.copyFrom (c, 0, audio, c, 0, numSamples);
    emptyMidi.clear();
    plugin.processBlock (work, emptyMidi);
    const int outs = juce::jmax (1, plugin.getTotalNumOutputChannels());
    for (int c = 0; c < 2; ++c)
        audio.copyFrom (c, 0, work, juce::jmin (c, outs - 1), 0, numSamples);
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
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

    syncToLink (numSamples);
    session.process (numSamples, live, trackMidi);

    for (auto& p : portBuffers)
        p.second.clear();

    // Chaque piste est une voie de console : source (instrument, ou retour audio d'un synthé
    // externe), égaliseur, inserts, volume et panoramique, puis le bus master.
    const bool canMix = numSamples <= masterBuffer.getNumSamples();
    // Les vu-mètres retombent de 20 dB par seconde environ.
    const float meterDecay = (float) std::pow (0.1, numSamples / sampleRate);
    juce::AudioBuffer<float> mix (masterBuffer.getArrayOfWritePointers(), 2, canMix ? numSamples : 0);
    mix.clear();
    bool anySolo = false;
    for (const auto& tr : song.tracks)
        anySolo = anySolo || tr.solo;

    for (int t = 0; t < song.numTracks() && t < (int) outputs.size(); ++t)
    {
        auto& out = *outputs[(size_t) t];
        auto& midi = trackMidi[(size_t) t];
        const auto& tr = song.tracks[(size_t) t];

        if (out.port != nullptr)
        {
            auto it = std::find_if (portBuffers.begin(), portBuffers.end(), [&] (const auto& p) { return p.first == out.port; });
            if (it != portBuffers.end())
            {
                // Volume de la piste en CC 7, envoyé quand il change (sauf si son retour audio est mixé ici).
                const int volumeCC = tr.audioInput >= 0 ? 100 : juce::jlimit (0, 127, juce::roundToInt (tr.volume * 127.0f));
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

        const bool hasReturn = tr.audioInput >= 0 && tr.audioInput < numInputChannels && inputChannelData[tr.audioInput] != nullptr;
        if ((out.plugin == nullptr && ! hasReturn) || ! canMix)
        {
            out.peak *= meterDecay;
            continue;
        }

        juce::AudioBuffer<float> audio (trackBuffer.getArrayOfWritePointers(), 2, numSamples);
        audio.clear();
        if (out.plugin != nullptr)
        {
            auto& plugin = *out.plugin;
            const int channels = juce::jmax (2, plugin.getTotalNumInputChannels(), plugin.getTotalNumOutputChannels());
            if (channels <= pluginBuffer.getNumChannels() && numSamples <= pluginBuffer.getNumSamples())
            {
                juce::AudioBuffer<float> work (pluginBuffer.getArrayOfWritePointers(), channels, numSamples);
                work.clear();
                plugin.processBlock (work, midi);
                const int outs = juce::jmax (1, plugin.getTotalNumOutputChannels());
                for (int c = 0; c < 2; ++c)
                    audio.copyFrom (c, 0, work, juce::jmin (c, outs - 1), 0, numSamples);
            }
        }
        if (hasReturn)
        {
            const float* left = inputChannelData[tr.audioInput];
            const float* right = tr.audioInputStereo && tr.audioInput + 1 < numInputChannels && inputChannelData[tr.audioInput + 1] != nullptr
                                     ? inputChannelData[tr.audioInput + 1] : left;
            audio.addFrom (0, 0, left, numSamples);
            audio.addFrom (1, 0, right, numSamples);
        }

        out.eq.process (audio, numSamples, tr.eq);
        for (size_t i = 0; i < out.inserts.size() && i < tr.inserts.size(); ++i)
            if (out.inserts[i] != nullptr && ! tr.inserts[i].bypass)
                runInsert (*out.inserts[i], audio, numSamples);

        const bool silent = tr.mute || (anySolo && ! tr.solo);
        const float gain = silent ? 0.0f : volumeToGain (tr.volume);
        const float pan = juce::jlimit (-1.0f, 1.0f, tr.pan);
        const float gainL = gain * (pan > 0.0f ? 1.0f - pan : 1.0f);
        const float gainR = gain * (pan < 0.0f ? 1.0f + pan : 1.0f);
        audio.applyGain (0, 0, numSamples, gainL);
        audio.applyGain (1, 0, numSamples, gainR);
        out.peak = juce::jmax (out.peak * meterDecay, audio.getMagnitude (0, 0, numSamples), audio.getMagnitude (1, 0, numSamples));
        mix.addFrom (0, 0, audio, 0, 0, numSamples);
        mix.addFrom (1, 0, audio, 1, 0, numSamples);
    }

    // Bus master : égaliseur, inserts, volume.
    if (canMix)
    {
        masterEqProcessor.process (mix, numSamples, song.masterEq);
        for (size_t i = 0; i < masterInsertInstances.size() && i < song.masterInserts.size(); ++i)
            if (masterInsertInstances[i] != nullptr && ! song.masterInserts[i].bypass)
                runInsert (*masterInsertInstances[i], mix, numSamples);
        mix.applyGain (volumeToGain (song.masterVolume));
        masterPeak = juce::jmax (masterPeak * meterDecay, mix.getMagnitude (0, 0, numSamples), mix.getMagnitude (1, 0, numSamples));
        for (int c = 0; c < juce::jmin (2, numOutputChannels); ++c)
            if (outputChannelData[c] != nullptr)
                juce::FloatVectorOperations::copy (outputChannelData[c], mix.getReadPointer (c), numSamples);
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
        outputs[(size_t) index]->eq.prepare (sampleRate);
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
    for (auto& insert : removed->inserts)
        if (insert != nullptr && onPluginRemoving)
            onPluginRemoving (insert.get());
    updateExcludedInputs();
}

void AudioEngine::setSong (Song song)
{
    std::vector<std::unique_ptr<TrackOutput>> old;
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> oldMaster;
    {
        const juce::ScopedLock sl (lock);
        for (int t = 0; t < session.song.numTracks(); ++t)
            if (auto* port = outputs[(size_t) t]->port)
                sendNotesOff (port, outputs[(size_t) t]->channel, session.takeAllNotesOff (t));
        old = std::move (outputs);
        outputs.clear();
        oldMaster = std::move (masterInsertInstances);
        masterInsertInstances.clear();
        session.setSong (std::move (song));
        session.prepare (sampleRate);
        masterEqProcessor.prepare (sampleRate);
        for (int t = 0; t < session.song.numTracks(); ++t)
        {
            outputs.push_back (std::make_unique<TrackOutput>());
            outputs.back()->eq.prepare (sampleRate);
            outputs.back()->inserts.resize (session.song.tracks[(size_t) t].inserts.size());
        }
        masterInsertInstances.resize (session.song.masterInserts.size());
        trackMidi.resize (outputs.size());
        changeCount = 0;
    }
    if (onPluginRemoving)
    {
        for (auto& o : old)
        {
            if (o->plugin != nullptr)
                onPluginRemoving (o->plugin.get());
            for (auto& insert : o->inserts)
                if (insert != nullptr)
                    onPluginRemoving (insert.get());
        }
        for (auto& insert : oldMaster)
            if (insert != nullptr)
                onPluginRemoving (insert.get());
    }
    old.clear();
    oldMaster.clear();

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

    // Inserts des pistes et du master, retours audio.
    auto loadInserts = [this] (int target, const std::vector<InsertSlot>& slots)
    {
        for (int i = 0; i < (int) slots.size(); ++i)
        {
            if (slots[(size_t) i].isEmpty())
                continue;
            juce::PluginDescription description;
            if (auto xml = juce::parseXML (slots[(size_t) i].description); xml != nullptr && description.loadFromXml (*xml))
                addInsert (target, i, description, slots[(size_t) i].state, false);
            else if (onError)
                onError ("Effet introuvable : " + slots[(size_t) i].name);
        }
    };
    int inputsNeeded = 0;
    for (int t = 0; t < copy.numTracks(); ++t)
    {
        loadInserts (t, copy.tracks[(size_t) t].inserts);
        const auto& tr = copy.tracks[(size_t) t];
        if (tr.audioInput >= 0)
            inputsNeeded = juce::jmax (inputsNeeded, tr.audioInput + (tr.audioInputStereo ? 2 : 1));
    }
    loadInserts (masterTarget, copy.masterInserts);
    if (inputsNeeded > 0)
        ensureAudioInputs (inputsNeeded);
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
    // États des inserts (pistes et master).
    const int tracks = read ([] (const Session& s) { return s.song.numTracks(); });
    for (int target = -1; target < tracks; ++target)
    {
        const int count = read ([&] (const Session&) { auto* v = instanceInserts (target); return v != nullptr ? (int) v->size() : 0; });
        for (int i = 0; i < count; ++i)
            if (auto* insert = getInsert (target, i))
            {
                juce::MemoryBlock state;
                insert->getStateInformation (state);
                const juce::ScopedLock sl (lock);
                if (auto* slots = modelInserts (target); slots != nullptr && i < (int) slots->size())
                    (*slots)[(size_t) i].state = state;
            }
    }

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

namespace gridplay
{
//==============================================================================
// Inserts
std::vector<InsertSlot>* AudioEngine::modelInserts (int target)
{
    if (target == masterTarget)
        return &session.song.masterInserts;
    return juce::isPositiveAndBelow (target, session.song.numTracks()) ? &session.song.tracks[(size_t) target].inserts : nullptr;
}

std::vector<std::unique_ptr<juce::AudioPluginInstance>>* AudioEngine::instanceInserts (int target)
{
    if (target == masterTarget)
        return &masterInsertInstances;
    return juce::isPositiveAndBelow (target, (int) outputs.size()) ? &outputs[(size_t) target]->inserts : nullptr;
}

void AudioEngine::addInsert (int target, int slot, const juce::PluginDescription& description, juce::MemoryBlock state, bool userChange)
{
    // On retrouve la piste par son identifiant : elle a pu bouger pendant le chargement.
    const int trackId = target == masterTarget ? 0
                      : read ([target] (const Session& s) { return juce::isPositiveAndBelow (target, s.song.numTracks()) ? s.song.tracks[(size_t) target].id : -1; });
    if (trackId < 0)
        return;

    auto safe = alive;
    formatManager.createPluginInstanceAsync (description, sampleRate, blockSize,
        [this, safe, target, trackId, slot, description, state, userChange] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
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
            prepareInstance (*instance);
            if (state.getSize() > 0)
                instance->setStateInformation (state.getData(), (int) state.getSize());

            std::unique_ptr<juce::AudioPluginInstance> old;
            {
                const juce::ScopedLock sl (lock);
                const int t = target == masterTarget ? masterTarget : indexOfTrack (trackId);
                auto* slots = modelInserts (t);
                auto* instances = instanceInserts (t);
                if (slots == nullptr || instances == nullptr)
                    return;
                const int needed = juce::jmax (2, instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels());
                if (needed > pluginBuffer.getNumChannels())
                    pluginBuffer.setSize (needed, juce::jmax (blockSize, pluginBuffer.getNumSamples()));
                // N'importe quelle case, comme dans Cubase : les cases d'avant restent vides.
                if (! juce::isPositiveAndBelow (slot, maxInserts))
                    return;
                if ((int) slots->size() <= slot)
                    slots->resize ((size_t) slot + 1);
                instances->resize (slots->size());
                const int index = slot;
                auto& model = (*slots)[(size_t) index];
                if (userChange)
                {
                    model.bypass = false;
                    ++changeCount;
                }
                model.name = description.name;
                if (auto xml = description.createXml())
                    model.description = xml->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader());
                old = std::move ((*instances)[(size_t) index]);
                (*instances)[(size_t) index] = std::move (instance);
            }
            if (old != nullptr && onPluginRemoving)
                onPluginRemoving (old.get());
        });
}

void AudioEngine::removeInsert (int target, int slot)
{
    std::unique_ptr<juce::AudioPluginInstance> old;
    {
        const juce::ScopedLock sl (lock);
        auto* slots = modelInserts (target);
        auto* instances = instanceInserts (target);
        if (slots == nullptr || instances == nullptr || ! juce::isPositiveAndBelow (slot, (int) slots->size()))
            return;
        // La case se vide, les autres effets gardent leur place.
        instances->resize (slots->size());
        old = std::move ((*instances)[(size_t) slot]);
        (*slots)[(size_t) slot] = {};
        trimEmptyInserts (*slots);
        instances->resize (slots->size());
        ++changeCount;
    }
    if (old != nullptr && onPluginRemoving)
        onPluginRemoving (old.get());
}

void AudioEngine::moveInsert (int target, int from, int to)
{
    const juce::ScopedLock sl (lock);
    auto* slots = modelInserts (target);
    auto* instances = instanceInserts (target);
    if (slots == nullptr || instances == nullptr || ! juce::isPositiveAndBelow (from, (int) slots->size())
        || ! juce::isPositiveAndBelow (to, maxInserts) || from == to)
        return;
    instances->resize (slots->size());
    // Vers une case vide : l'effet y va, les autres ne bougent pas. Vers une case occupée :
    // il s'intercale à cette place et les suivants descendent d'un cran.
    const bool targetEmpty = to >= (int) slots->size() || (*slots)[(size_t) to].isEmpty();
    if (targetEmpty)
    {
        slots->resize (juce::jmax (slots->size(), (size_t) to + 1));
        instances->resize (slots->size());
        std::swap ((*slots)[(size_t) from], (*slots)[(size_t) to]);
        std::swap ((*instances)[(size_t) from], (*instances)[(size_t) to]);
    }
    else
    {
        moveSlot (*slots, from, to);
        moveSlot (*instances, from, to);
        // Une case vide plus bas absorbe le décalage, pour ne pas dépasser 8.
        for (int i = to + 1; i < (int) slots->size() && (int) slots->size() > maxInserts; ++i)
            if ((*slots)[(size_t) i].isEmpty())
            {
                slots->erase (slots->begin() + i);
                instances->erase (instances->begin() + i);
                break;
            }
    }
    trimEmptyInserts (*slots);
    instances->resize (slots->size());
    ++changeCount;
}

juce::AudioPluginInstance* AudioEngine::getInsert (int target, int slot)
{
    const juce::ScopedLock sl (lock);
    auto* instances = instanceInserts (target);
    return instances != nullptr && juce::isPositiveAndBelow (slot, (int) instances->size()) ? (*instances)[(size_t) slot].get() : nullptr;
}

//==============================================================================
// Retours audio et vu-mètres
juce::StringArray AudioEngine::getAudioInputNames() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getInputChannelNames();
    return {};
}

void AudioEngine::ensureAudioInputs (int channelsNeeded)
{
    // Les entrées ne s'ouvrent qu'à la demande (macOS demande alors l'accès au micro).
    auto setup = deviceManager.getAudioDeviceSetup();
    const int available = getAudioInputNames().size();
    const int wanted = juce::jmin (channelsNeeded, available);
    if (wanted <= 0)
        return;
    bool missing = setup.useDefaultInputChannels;
    for (int c = 0; c < wanted; ++c)
        missing = missing || ! setup.inputChannels[c];
    if (! missing)
        return;
    setup.useDefaultInputChannels = false;
    setup.inputChannels.setRange (0, wanted, true);
    const auto error = deviceManager.setAudioDeviceSetup (setup, true);
    if (error.isNotEmpty() && onError)
        onError (juce::String::fromUTF8 ("Entrées de la carte son : ") + error);
}

std::vector<float> AudioEngine::getMeters() const
{
    const juce::ScopedLock sl (lock);
    std::vector<float> meters;
    meters.reserve (outputs.size() + 1);
    for (const auto& o : outputs)
        meters.push_back (o->peak);
    meters.push_back (masterPeak);
    return meters;
}
} // namespace gridplay

namespace gridplay
{
//==============================================================================
// Ableton Link
void AudioEngine::setLinkEnabled (bool enabled)
{
    link->enable (enabled);
    if (enabled)
    {
        // En rejoignant une session, on garde le tempo du groupe ; seul, on lui donne le nôtre.
        const double tempo = read ([] (const Session& s) { return s.song.tempo; });
        if (link->numPeers() == 0)
        {
            auto state = link->captureAppSessionState();
            state.setTempo (tempo, link->clock().micros());
            link->commitAppSessionState (state);
        }
    }
}

bool AudioEngine::isLinkEnabled() const   { return link->isEnabled(); }
int AudioEngine::getLinkPeers() const     { return (int) link->numPeers(); }

void AudioEngine::setTempo (double bpm)
{
    edit ([bpm] (Session& s) { s.song.tempo = bpm; return 0; });
    if (link->isEnabled())
    {
        auto state = link->captureAppSessionState();
        state.setTempo (bpm, link->clock().micros());
        link->commitAppSessionState (state);
    }
}

void AudioEngine::syncToLink (int numSamples)
{
    // Appelé dans le fil audio, sous le verrou, au début de chaque bloc.
    session.setRateScale (1.0);
    if (! link->isEnabled())
    {
        linkWasPlaying = false;
        return;
    }

    const auto state = link->captureAudioSessionState();
    const auto hostTime = std::chrono::microseconds (link->clock().micros().count() + (long long) outputLatencyMicros);
    const double quantum = beatsPerBar;
    auto& song = session.song;
    if (std::abs (state.tempo() - song.tempo) > 1.0e-6)
        song.tempo = state.tempo();

    if (! session.isPlaying())
    {
        linkWasPlaying = false;
        return;
    }

    const double linkPhase = state.phaseAtTime (hostTime, quantum);
    if (! linkWasPlaying)
    {
        linkWasPlaying = true;
        // Démarrage : on attend la prochaine mesure du groupe, comme Ableton Live
        // (le transport part « avant » 0 et la grille démarre pile sur le temps 1).
        if (session.getBeat() < 1.0e-9)
        {
            session.setBeat (linkPhase - quantum);
            return;
        }
    }

    // En lecture : même phase dans la mesure que le groupe.
    const double ours = session.getBeat() - std::floor (session.getBeat() / quantum) * quantum;
    double error = linkPhase - ours;
    if (error > quantum * 0.5) error -= quantum;
    if (error < -quantum * 0.5) error += quantum;

    const double blockBeats = numSamples / (sampleRate * 60.0 / song.tempo);
    if (std::abs (error) > 0.25)
        session.setBeat (session.getBeat() + error);                                  // gros écart : on saute
    else if (std::abs (error) > 1.0e-6)
        session.setRateScale (juce::jlimit (0.95, 1.05, 1.0 + error / (blockBeats * 16.0)));  // petit écart : on rattrape doucement
}

std::pair<double, double> AudioEngine::getLinkPhases()
{
    const juce::ScopedLock sl (lock);
    const auto state = link->captureAppSessionState();
    const auto now = std::chrono::microseconds (link->clock().micros().count() + (long long) outputLatencyMicros);
    const double beat = session.getBeat();
    return { beat - std::floor (beat / beatsPerBar) * beatsPerBar, state.phaseAtTime (now, beatsPerBar) };
}
} // namespace gridplay
