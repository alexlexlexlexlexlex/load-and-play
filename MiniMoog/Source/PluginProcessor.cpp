#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    juce::String formatTime (float seconds)
    {
        return seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms"
                              : juce::String (seconds, 2) + " s";
    }

    juce::String formatHz (float hz)
    {
        return hz < 1000.0f ? juce::String (juce::roundToInt (hz)) + " Hz"
                            : juce::String (hz / 1000.0f, 2) + " kHz";
    }

    void addFloat (Layout& layout, const char* id, const char* name, juce::NormalisableRange<float> range,
                   float defaultValue, std::function<juce::String (float)> toText = nullptr)
    {
        if (toText == nullptr)
            toText = [] (float v) { return juce::String (v, 1); };

        auto attributes = juce::AudioParameterFloatAttributes()
                              .withStringFromValueFunction ([toText] (float v, int) { return toText (v); });

        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range,
                                                                 defaultValue, attributes));
    }

    void addBool (Layout& layout, const char* id, const char* name, bool defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, defaultValue));
    }

    void addChoice (Layout& layout, const char* id, const char* name, const juce::StringArray& choices, int defaultIndex)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, defaultIndex));
    }

    juce::String formatCents (float semitones)
    {
        return juce::String (juce::roundToInt (semitones * 100.0f)) + " ct";
    }

    juce::NormalisableRange<float> logRange (float minimum, float maximum, float centre)
    {
        juce::NormalisableRange<float> range (minimum, maximum);
        range.setSkewForCentre (centre);
        return range;
    }

    juce::NormalisableRange<float> timeRange (float minSeconds, float maxSeconds)
    {
        juce::NormalisableRange<float> range (minSeconds, maxSeconds);
        range.setSkewForCentre (std::sqrt (minSeconds * maxSeconds) * 2.0f);
        return range;
    }
}

//==============================================================================
ModelDProcessor::ModelDProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ModelD", createLayout())
{
    for (auto* p : getParameters())
        parameters.add (dynamic_cast<juce::RangedAudioParameter*> (p));

    for (int i = 0; i < 16; ++i)
        arpSteps[(size_t) i] = apvts.getRawParameterValue ("arpStep" + juce::String (i + 1));

    for (auto& cc : ccMap)     cc = -1;
    for (auto& cc : pendingCc) cc = -1;

    startTimerHz (30);
}

ModelDProcessor::~ModelDProcessor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout ModelDProcessor::createLayout()
{
    Layout layout;

    const juce::StringArray ranges { "LO", "32'", "16'", "8'", "4'", "2'" };
    const juce::StringArray waves { "Triangle", "Tri-Saw", "Saw", "Square", "Wide Pulse", "Narrow Pulse" };
    const juce::StringArray waves3 { "Triangle", "Rev Saw", "Saw", "Square", "Wide Pulse", "Narrow Pulse" };
    const juce::NormalisableRange<float> zeroToTen (0.0f, 10.0f);

    auto semitones = [] (float v) { return juce::String (v, 2) + " st"; };

    //--------------------------------------------------------------------------
    // Façade du Model D

    // Controllers
    addFloat (layout, "tune", "Tune", { -1.0f, 1.0f }, 0.0f, semitones);
    addFloat (layout, "glide", "Glide", juce::NormalisableRange<float> (0.0f, 5.0f, 0.0f, 0.3f), 0.0f, formatTime);
    addFloat (layout, "modMix", "Modulation Mix", zeroToTen, 0.0f);
    addBool (layout, "oscMod", "Oscillator Modulation", false);
    addBool (layout, "osc3Ctrl", "Osc 3 Control", true);

    // Oscillator bank
    addChoice (layout, "osc1Range", "Osc 1 Range", ranges, 3);
    addChoice (layout, "osc1Wave", "Osc 1 Waveform", waves, 2);
    addChoice (layout, "osc2Range", "Osc 2 Range", ranges, 3);
    addFloat (layout, "osc2Freq", "Osc 2 Frequency", { -7.0f, 7.0f }, 0.1f, semitones);
    addChoice (layout, "osc2Wave", "Osc 2 Waveform", waves, 2);
    addChoice (layout, "osc3Range", "Osc 3 Range", ranges, 3);
    addFloat (layout, "osc3Freq", "Osc 3 Frequency", { -7.0f, 7.0f }, -0.1f, semitones);
    addChoice (layout, "osc3Wave", "Osc 3 Waveform", waves3, 0);

    // Mixer
    addBool (layout, "osc1On", "Osc 1 On", true);
    addFloat (layout, "osc1Level", "Osc 1 Volume", zeroToTen, 8.0f);
    addBool (layout, "osc2On", "Osc 2 On", true);
    addFloat (layout, "osc2Level", "Osc 2 Volume", zeroToTen, 8.0f);
    addBool (layout, "osc3On", "Osc 3 On", false);
    addFloat (layout, "osc3Level", "Osc 3 Volume", zeroToTen, 5.0f);
    addBool (layout, "noiseOn", "Noise On", false);
    addFloat (layout, "noiseLevel", "Noise Volume", zeroToTen, 0.0f);
    addBool (layout, "pinkNoise", "Pink Noise", false);

    // Modifiers
    addFloat (layout, "cutoff", "Cutoff Frequency", logRange (20.0f, 20000.0f, 630.0f), 1200.0f, formatHz);
    addFloat (layout, "emphasis", "Emphasis", zeroToTen, 2.0f);
    addFloat (layout, "contour", "Amount of Contour", zeroToTen, 5.0f);
    addBool (layout, "filterMod", "Filter Modulation", false);
    addBool (layout, "kbd1", "Keyboard Control 1", false);
    addBool (layout, "kbd2", "Keyboard Control 2", false);
    addBool (layout, "bassComp", "Bass Compensation", false);
    addFloat (layout, "fAttack", "Filter Attack", timeRange (0.001f, 10.0f), 0.005f, formatTime);
    addFloat (layout, "fDecay", "Filter Decay", timeRange (0.004f, 35.0f), 0.4f, formatTime);
    addFloat (layout, "fSustain", "Filter Sustain", zeroToTen, 3.0f);
    addFloat (layout, "lAttack", "Loudness Attack", timeRange (0.001f, 10.0f), 0.002f, formatTime);
    addFloat (layout, "lDecay", "Loudness Decay", timeRange (0.004f, 35.0f), 0.3f, formatTime);
    addFloat (layout, "lSustain", "Loudness Sustain", zeroToTen, 8.0f);
    addBool (layout, "decay", "Decay", false);

    // Output
    addFloat (layout, "volume", "Volume", zeroToTen, 7.0f);
    addFloat (layout, "bendRange", "Pitch Bend Range", juce::NormalisableRange<float> (1.0f, 12.0f, 1.0f), 2.0f,
              [] (float v) { return juce::String (juce::roundToInt (v)) + " st"; });
    addFloat (layout, "drift", "Analog Drift", zeroToTen, 3.0f);

    //--------------------------------------------------------------------------
    // Options supplémentaires : leurs valeurs par défaut ne changent rien au son.

    // Jeu
    addBool (layout, "retrigger", "Retrigger", false);
    addBool (layout, "poly", "Polyphonic", false);
    addFloat (layout, "aftertouch", "Aftertouch Amount", zeroToTen, 0.0f);
    addFloat (layout, "velocity", "Velocity to Filter", zeroToTen, 0.0f);
    addFloat (layout, "modAmount", "Modulation Amount", zeroToTen, 0.0f);
    addBool (layout, "modFromLfo", "Modulation from LFO", false);
    addFloat (layout, "bpm", "Tempo (without host)", juce::NormalisableRange<float> (40.0f, 240.0f, 1.0f), 120.0f,
              [] (float v) { return juce::String (juce::roundToInt (v)) + " BPM"; });

    // Oscillateurs
    addFloat (layout, "osc1Freq", "Osc 1 Frequency", { -7.0f, 7.0f }, 0.0f, semitones);
    addFloat (layout, "osc2Fine", "Osc 2 Fine Tune", { -0.5f, 0.5f }, 0.0f, formatCents);
    addFloat (layout, "osc3Fine", "Osc 3 Fine Tune", { -0.5f, 0.5f }, 0.0f, formatCents);
    for (int i = 1; i <= 3; ++i)
    {
        const auto n = juce::String (i);
        addBool (layout, ("osc" + n + "Rect").toRawUTF8(), ("Osc " + n + " Rectify").toRawUTF8(), false);
        addFloat (layout, ("osc" + n + "Offset").toRawUTF8(), ("Osc " + n + " Offset").toRawUTF8(),
                  { -1.0f, 1.0f }, 0.0f, [] (float v) { return juce::String (v, 2); });
    }
    addBool (layout, "oscSync", "Osc Sync 2 to 1", false);

    // LFO
    addFloat (layout, "lfoLevel", "LFO Level", zeroToTen, 0.0f);
    addChoice (layout, "lfoWave", "LFO Waveform", { "Triangle", "Square", "Saw Up", "Saw Down", "Sample & Hold" }, 0);
    addFloat (layout, "lfoRate", "LFO Rate", logRange (0.05f, 30.0f, 2.0f), 2.0f,
              [] (float v) { return juce::String (v, v < 10.0f ? 2 : 1) + " Hz"; });
    addBool (layout, "lfoSync", "LFO Tempo Sync", false);
    addChoice (layout, "lfoDivision", "LFO Sync Rate", modeld::divisionNames(), 4);
    addChoice (layout, "lfoDest", "LFO Destination", { "All Osc", "Osc 2", "Filter", "Volume" }, 0);

    // Filtre et contours
    addFloat (layout, "vco3Filter", "VCO 3 Filter Mod", zeroToTen, 0.0f);
    addBool (layout, "adsr", "ADSR Release", false);
    addFloat (layout, "fRelease", "Filter Release", timeRange (0.004f, 35.0f), 0.3f, formatTime);
    addFloat (layout, "lRelease", "Loudness Release", timeRange (0.004f, 35.0f), 0.3f, formatTime);

    // Effets
    addBool (layout, "driveOn", "Overdrive On", false);
    addFloat (layout, "driveGain", "Overdrive Gain", zeroToTen, 5.0f);
    addFloat (layout, "driveLowCut", "Overdrive Low Cut", logRange (20.0f, 1000.0f, 150.0f), 20.0f, formatHz);
    addFloat (layout, "driveHighCut", "Overdrive High Cut", logRange (1000.0f, 20000.0f, 5000.0f), 20000.0f, formatHz);
    addChoice (layout, "chorusMode", "Chorus Mode", { "Off", "I", "II", "I+II" }, 0);
    addFloat (layout, "chorusLevel", "Chorus Level", zeroToTen, 10.0f);
    addBool (layout, "autopan", "Autopan", false);
    addBool (layout, "delayOn", "Delay On", false);
    addFloat (layout, "delayLevel", "Delay Level", zeroToTen, 3.0f);
    addFloat (layout, "delayTime", "Delay Time", logRange (0.01f, 2.0f, 0.3f), 0.35f, formatTime);
    addBool (layout, "delaySync", "Delay Tempo Sync", false);
    addChoice (layout, "delayDivision", "Delay Sync Time", modeld::divisionNames(), 5);
    addBool (layout, "delayRight2x", "Delay Right 2x", false);
    addFloat (layout, "delayFeedback", "Delay Feedback", zeroToTen, 4.0f);

    // Arpégiateur
    addBool (layout, "arpOn", "Arpeggiator On", false);
    addChoice (layout, "arpMode", "Arpeggiator Mode", modeld::Arpeggiator::modeNames(), 0);
    addChoice (layout, "arpDivision", "Arpeggiator Speed", modeld::divisionNames(), 5);
    addChoice (layout, "arpRange", "Arpeggiator Range", { "1 Oct", "2 Oct", "3 Oct", "4 Oct" }, 0);
    addBool (layout, "arpGate", "Arpeggiator Gate Pattern", false);
    for (int i = 1; i <= 16; ++i)
        addBool (layout, ("arpStep" + juce::String (i)).toRawUTF8(), ("Arp Step " + juce::String (i)).toRawUTF8(), true);

    return layout;
}

modeld::Params ModelDProcessor::readParams() const
{
    modeld::Params p;

    p.tune = param ("tune");
    p.glide = param ("glide");
    p.modMix = param ("modMix");
    p.oscMod = flag ("oscMod");
    p.osc3Ctrl = flag ("osc3Ctrl");

    p.range = { choice ("osc1Range"), choice ("osc2Range"), choice ("osc3Range") };
    p.wave = { choice ("osc1Wave"), choice ("osc2Wave"), choice ("osc3Wave") };
    p.osc2Freq = param ("osc2Freq");
    p.osc3Freq = param ("osc3Freq");

    p.oscOn = { flag ("osc1On"), flag ("osc2On"), flag ("osc3On") };
    p.oscLevel = { param ("osc1Level"), param ("osc2Level"), param ("osc3Level") };
    p.noiseOn = flag ("noiseOn");
    p.noiseLevel = param ("noiseLevel");
    p.pinkNoise = flag ("pinkNoise");

    p.cutoff = param ("cutoff");
    p.emphasis = param ("emphasis");
    p.contour = param ("contour");
    p.filterMod = flag ("filterMod");
    p.kbd1 = flag ("kbd1");
    p.kbd2 = flag ("kbd2");
    p.bassComp = flag ("bassComp");
    p.filterAttack = param ("fAttack");
    p.filterDecay = param ("fDecay");
    p.filterSustain = param ("fSustain");
    p.loudAttack = param ("lAttack");
    p.loudDecay = param ("lDecay");
    p.loudSustain = param ("lSustain");
    p.decaySwitch = flag ("decay");

    p.volume = param ("volume");
    p.bendRange = param ("bendRange");
    p.drift = param ("drift");

    p.poly = flag ("poly");
    p.retrigger = flag ("retrigger");
    p.aftertouch = param ("aftertouch");
    p.velocity = param ("velocity");
    p.modAmount = param ("modAmount");
    p.modFromLfo = flag ("modFromLfo");

    p.osc1Freq = param ("osc1Freq");
    p.osc2Fine = param ("osc2Fine");
    p.osc3Fine = param ("osc3Fine");
    p.rectify = { flag ("osc1Rect"), flag ("osc2Rect"), flag ("osc3Rect") };
    p.offset = { param ("osc1Offset"), param ("osc2Offset"), param ("osc3Offset") };
    p.oscSync = flag ("oscSync");

    p.lfoLevel = param ("lfoLevel");
    p.lfoWave = choice ("lfoWave");
    p.lfoRate = param ("lfoRate");
    p.lfoSync = flag ("lfoSync");
    p.lfoDivision = choice ("lfoDivision");
    p.lfoDest = choice ("lfoDest");

    p.vco3Filter = param ("vco3Filter");
    p.adsr = flag ("adsr");
    p.filterRelease = param ("fRelease");
    p.loudRelease = param ("lRelease");
    return p;
}

modeld::EffectsParams ModelDProcessor::readEffects() const
{
    modeld::EffectsParams e;
    e.driveOn = flag ("driveOn");
    e.driveGain = param ("driveGain");
    e.driveLowCut = param ("driveLowCut");
    e.driveHighCut = param ("driveHighCut");
    e.chorusMode = choice ("chorusMode");
    e.chorusLevel = param ("chorusLevel");
    e.autopan = flag ("autopan");
    e.delayOn = flag ("delayOn");
    e.delayLevel = param ("delayLevel");
    e.delayTime = param ("delayTime");
    e.delaySync = flag ("delaySync");
    e.delayDivision = choice ("delayDivision");
    e.delayRightDouble = flag ("delayRight2x");
    e.delayFeedback = param ("delayFeedback");
    return e;
}

modeld::ArpSettings ModelDProcessor::readArp() const
{
    modeld::ArpSettings a;
    a.on = flag ("arpOn");
    a.mode = choice ("arpMode");
    a.division = choice ("arpDivision");
    a.range = choice ("arpRange") + 1;
    a.gate = flag ("arpGate");
    for (int i = 0; i < 16; ++i)
        a.steps[(size_t) i] = arpSteps[(size_t) i]->load() > 0.5f;
    return a;
}

//==============================================================================
void ModelDProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate, samplesPerBlock);
    effects.prepare (sampleRate);
    arpeggiator.prepare (sampleRate);
    stereo.setSize (1, samplesPerBlock);
    fromUi.ensureSize (1024);
    uiMidi.reset (sampleRate);
    midiPlayer.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
    keyboardState.reset();
}

bool ModelDProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void ModelDProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isControllerOfType (1))
        engine.setModWheel ((float) m.getControllerValue() / 127.0f);
    else if (m.isChannelPressure())
        engine.setAftertouch ((float) m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())
        engine.setAftertouch ((float) m.getAfterTouchValue() / 127.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
}

// Contrôleurs MIDI associés aux réglages. Les valeurs sont appliquées par le
// timer, hors du thread audio.
void ModelDProcessor::handleControllers (const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        if (! m.isController() || m.getControllerNumber() >= 120)
            continue;

        const int cc = m.getControllerNumber();
        const int target = learnTarget.exchange (-1);

        if (target >= 0)
        {
            for (auto& mapped : ccMap)
                if (mapped == target)
                    mapped = -1;
            ccMap[(size_t) cc] = target;
            lastLearnedCc = cc;
            lastLearnedParam = target;
        }

        if (ccMap[(size_t) cc] >= 0)
            pendingCc[(size_t) cc] = m.getControllerValue();
    }
}

void ModelDProcessor::timerCallback()
{
    for (size_t cc = 0; cc < 128; ++cc)
    {
        const int value = pendingCc[cc].exchange (-1);
        const int index = ccMap[cc].load();
        if (value >= 0 && index >= 0)
            if (auto* p = parameters[index])
                p->setValueNotifyingHost ((float) value / 127.0f);
    }
}

void ModelDProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    // Fichier MIDI glissé sur la fenêtre (il joue comme le clavier), notes du
    // clavier à l'écran et mouvements des molettes de l'interface.
    midiPlayer.process (midi, numSamples);
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
    fromUi.clear();
    uiMidi.removeNextBlockOfMessages (fromUi, numSamples);
    midi.addEvents (fromUi, 0, numSamples, 0);

    handleControllers (midi);

    // Tempo de l'hôte, sinon le réglage TEMPO.
    double bpm = param ("bpm"), ppq = 0.0;
    bool playing = false;
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto hostBpm = position->getBpm())
                bpm = *hostBpm;
            if (auto hostPpq = position->getPpqPosition())
            {
                ppq = *hostPpq;
                playing = position->getIsPlaying();
            }
        }
    }

    arpeggiator.process (midi, numSamples, readArp(), bpm, playing, ppq);

    const auto params = readParams();
    engine.setParams (params);
    engine.setTempo (bpm);
    engine.setTransport (playing, ppq);

    auto* out = buffer.getWritePointer (0);
    int position = 0;

    for (const auto metadata : midi)
    {
        const int eventTime = juce::jlimit (0, numSamples, metadata.samplePosition);
        if (eventTime > position)
        {
            engine.render (out + position, eventTime - position);
            position = eventTime;
        }
        handleMidi (metadata.getMessage());
    }

    if (position < numSamples)
        engine.render (out + position, numSamples - position);

    // Effets. Tous coupés, la sortie est le synthé seul, copié sur chaque canal.
    const auto fx = readEffects();
    const float panHz = params.lfoSync ? (float) (bpm / 60.0 / modeld::divisionBeats (params.lfoDivision))
                                       : params.lfoRate;
    effects.setParams (fx, bpm, panHz);

    if (fx.anyOn())
    {
        if (buffer.getNumChannels() > 1)
        {
            effects.process (out, buffer.getWritePointer (1), numSamples);
        }
        else
        {
            stereo.setSize (1, numSamples, false, false, true);
            auto* right = stereo.getWritePointer (0);
            effects.process (out, right, numSamples);
            for (int i = 0; i < numSamples; ++i)
                out[i] = 0.5f * (out[i] + right[i]);
        }
    }
    else
    {
        for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
    }
}

//==============================================================================
int ModelDProcessor::indexOf (const juce::String& paramId) const
{
    for (int i = 0; i < parameters.size(); ++i)
        if (parameters[i] != nullptr && parameters[i]->getParameterID() == paramId)
            return i;
    return -1;
}

void ModelDProcessor::setLearnTarget (const juce::String& paramId)
{
    learnTarget = indexOf (paramId);
}

juce::String ModelDProcessor::getLearnTarget() const
{
    const int index = learnTarget.load();
    return index >= 0 ? parameters[index]->getParameterID() : juce::String();
}

void ModelDProcessor::clearMidiMap()
{
    for (auto& cc : ccMap)
        cc = -1;
    learnTarget = -1;
    lastLearnedCc = -1;
    lastLearnedParam = -1;
}

int ModelDProcessor::getCcFor (const juce::String& paramId) const
{
    const int index = indexOf (paramId);
    for (size_t cc = 0; cc < ccMap.size(); ++cc)
        if (index >= 0 && ccMap[cc] == index)
            return (int) cc;
    return -1;
}

juce::String ModelDProcessor::getLastLearned() const
{
    const int cc = lastLearnedCc.load();
    const int index = lastLearnedParam.load();
    if (cc < 0 || index < 0)
        return {};
    return "CC " + juce::String (cc) + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x92 ")) + parameters[index]->getName (64);
}

//==============================================================================
juce::AudioProcessorEditor* ModelDProcessor::createEditor()
{
    return new ModelDEditor (*this);
}

void ModelDProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Les associations MIDI learn voyagent avec le projet.
    state.removeChild (state.getChildWithName ("MIDIMAP"), nullptr);
    juce::ValueTree map ("MIDIMAP");
    for (size_t cc = 0; cc < ccMap.size(); ++cc)
        if (const int index = ccMap[cc]; index >= 0)
            map.setProperty ("cc" + juce::String ((int) cc), parameters[index]->getParameterID(), nullptr);
    state.appendChild (map, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ModelDProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (state);

    for (auto& cc : ccMap)
        cc = -1;

    const auto map = state.getChildWithName ("MIDIMAP");
    for (int i = 0; i < map.getNumProperties(); ++i)
    {
        const auto name = map.getPropertyName (i).toString();
        const int cc = name.substring (2).getIntValue();
        const int index = indexOf (map.getProperty (name).toString());
        if (cc >= 0 && cc < 128 && index >= 0)
            ccMap[(size_t) cc] = index;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ModelDProcessor();
}
