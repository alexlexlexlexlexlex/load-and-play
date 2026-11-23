#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

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

    juce::String formatTime (float seconds)
    {
        return seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms"
                              : juce::String (seconds, 2) + " s";
    }

    juce::String formatSemitones (float v)
    {
        const int st = juce::roundToInt (v);
        return (st > 0 ? "+" : "") + juce::String (st) + " st";
    }

    // Le réglage de transposition suit la dernière touche jouée pendant que le
    // séquenceur tourne : do 3 (C3, note 60) = pas de transposition.
    constexpr int transposeRoot = 60;
}

//==============================================================================
LP303Processor::LP303Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LP303", createLayout())
{
    lp303::loadFactoryPatterns (patterns);
    startTimerHz (30);
}

LP303Processor::~LP303Processor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout LP303Processor::createLayout()
{
    Layout layout;
    const juce::NormalisableRange<float> zeroToTen (0.0f, 10.0f);

    // Son
    addChoice (layout, "wave", "Waveform", { "Saw", "Square" }, 0);
    addFloat (layout, "tune", "Tuning", juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f,
              [] (float v) { return juce::String (v, 2) + " st"; });
    addFloat (layout, "cutoff", "Cutoff", zeroToTen, 3.5f, [] (float v)
              {
                  const float hz = 60.0f * std::exp2 (0.7f * v);
                  return hz < 1000.0f ? juce::String (juce::roundToInt (hz)) + " Hz" : juce::String (hz / 1000.0f, 2) + " kHz";
              });
    addFloat (layout, "resonance", "Resonance", zeroToTen, 7.0f);
    addFloat (layout, "envMod", "Env Mod", zeroToTen, 6.0f);
    addFloat (layout, "decay", "Decay", zeroToTen, 4.0f,
              [] (float v) { return formatTime (0.2f * std::pow (12.5f, v / 10.0f)); });
    addFloat (layout, "accent", "Accent", zeroToTen, 6.0f);
    addFloat (layout, "slide", "Slide Time", juce::NormalisableRange<float> (0.02f, 0.5f, 0.0f, 0.5f), 0.08f, formatTime);
    addFloat (layout, "drive", "Drive", zeroToTen, 0.0f);
    addFloat (layout, "volume", "Volume", zeroToTen, 7.0f);

    // Séquenceur
    addBool (layout, "seqOn", "Sequencer On", false);
    juce::StringArray patternNames;
    for (int i = 1; i <= lp303::numPatterns; ++i)
        patternNames.add (juce::String (i));
    addChoice (layout, "pattern", "Pattern", patternNames, 0);
    addFloat (layout, "transpose", "Transpose", juce::NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f, formatSemitones);
    addFloat (layout, "swing", "Swing", zeroToTen, 0.0f,
              [] (float v) { return juce::String (juce::roundToInt (50.0f + 2.5f * v)) + " %"; });
    addFloat (layout, "bpm", "Tempo (without host)", juce::NormalisableRange<float> (40.0f, 240.0f, 1.0f), 120.0f,
              [] (float v) { return juce::String (juce::roundToInt (v)) + " BPM"; });

    return layout;
}

lp303::Params LP303Processor::readParams() const
{
    lp303::Params p;
    p.wave = choice ("wave");
    p.tune = param ("tune");
    p.cutoff = param ("cutoff");
    p.resonance = param ("resonance");
    p.envMod = param ("envMod");
    p.decay = param ("decay");
    p.accent = param ("accent");
    p.slide = param ("slide");
    p.drive = param ("drive");
    p.volume = param ("volume");
    return p;
}

//==============================================================================
int LP303Processor::getNumPrograms()
{
    return (int) lp303::factoryPresets().size();
}

const juce::String LP303Processor::getProgramName (int index)
{
    const auto& presets = lp303::factoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? juce::String (presets[(size_t) index].name)
                                                                  : juce::String();
}

void LP303Processor::setCurrentProgram (int index)
{
    const auto& presets = lp303::factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;

    // Un preset règle le son ; le séquenceur et le tempo ne bougent pas.
    static const juce::StringArray keep { "seqOn", "pattern", "transpose", "swing", "bpm" };
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! keep.contains (ranged->getParameterID()))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

//==============================================================================
void LP303Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    rate = sampleRate;
    engine.setParams (readParams());
    engine.prepare (sampleRate, samplesPerBlock);
    sequencer.prepare (sampleRate);
    midiPlayer.prepare (sampleRate);
    forEngine.ensureSize (2048);
    setLatencySamples (engine.getLatencySamples());
    keyboardState.reset();
    wasRunning = false;
}

bool LP303Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void LP303Processor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
}

void LP303Processor::flushPendingChanges()
{
    // Transposition jouée au clavier pendant que le séquenceur tourne : reportée
    // sur le réglage (hors du thread audio) pour qu'on la voie et la sauvegarde.
    const int t = pendingTranspose.exchange (1000);
    if (t != 1000)
        if (auto* p = apvts.getParameter ("transpose"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) t));
}

void LP303Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    // Le fichier MIDI joue comme le clavier (ses notes s'allument à l'écran).
    midiPlayer.process (midi, numSamples);
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // Tempo et position de l'hôte ; sans hôte (application autonome), une
    // horloge interne au tempo du réglage TEMPO, qui part au lancement.
    double bpm = param ("bpm"), ppq = 0.0;
    bool playing = false, fromHost = false;
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
                fromHost = true;
            }
        }
    }

    const bool seqOn = flag ("seqOn");
    if (! fromHost)
    {
        if (seqOn && ! wasSeqOn)
            internalPpq = 0.0;
        ppq = internalPpq;
        playing = seqOn;
    }
    wasSeqOn = seqOn;
    hostTransport = fromHost;

    const bool running = seqOn && playing;
    seqRunning = running;

    // Transposition : le réglage, sauf si une touche vient de la changer.
    const int transposeParam = juce::roundToInt (param ("transpose"));
    if (transposeParam != lastTransposeParam)
        transposeNow = transposeParam;
    lastTransposeParam = transposeParam;

    // Pendant que le séquenceur joue, les touches transposent le motif au lieu
    // de jouer : seuls les autres messages (pitch bend...) vont au moteur.
    forEngine.clear();
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        if (running && m.isNoteOnOrOff())
        {
            if (m.isNoteOn())
            {
                transposeNow = juce::jlimit (-12, 12, m.getNoteNumber() - transposeRoot);
                pendingTranspose = transposeNow;
            }
            continue;
        }
        forEngine.addEvent (m, metadata.samplePosition);
    }

    if (running && ! wasRunning)
        engine.allNotesOff();             // touches tenues avant le départ
    wasRunning = running;

    lp303::SequencerSettings settings;
    settings.running = running;
    settings.pattern = choice ("pattern");
    settings.swing = param ("swing") / 10.0f;
    settings.transpose = transposeNow;
    sequencer.process (forEngine, numSamples, patterns, settings, bpm, ppq);

    if (! fromHost && running)
        internalPpq += numSamples * bpm / 60.0 / rate;

    engine.setParams (readParams());

    auto* out = buffer.getWritePointer (0);
    int position = 0;
    for (const auto metadata : forEngine)
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

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}

//==============================================================================
void LP303Processor::patternsEdited()
{
    // Les motifs ne sont pas des paramètres : on prévient l'hôte que le projet a changé.
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

juce::AudioProcessorEditor* LP303Processor::createEditor()
{
    return new LP303Editor (*this);
}

void LP303Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("seqShown", sequencerShown.load(), nullptr);

    state.removeChild (state.getChildWithName ("PATTERNS"), nullptr);
    juce::ValueTree bank ("PATTERNS");
    for (int p = 0; p < lp303::numPatterns; ++p)
        bank.setProperty ("p" + juce::String (p + 1), patterns.toString (p), nullptr);
    state.appendChild (bank, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LP303Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    currentProgram = state.getProperty ("program", 0);
    sequencerShown = (bool) state.getProperty ("seqShown", true);

    const auto bank = state.getChildWithName ("PATTERNS");
    for (int p = 0; p < lp303::numPatterns; ++p)
        if (bank.hasProperty ("p" + juce::String (p + 1)))
            patterns.fromString (p, bank.getProperty ("p" + juce::String (p + 1)).toString());

    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LP303Processor();
}
