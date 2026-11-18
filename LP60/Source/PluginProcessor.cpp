#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    juce::String formatTime (float seconds)
    {
        return seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms"
                              : juce::String (seconds, 2) + " s";
    }

    // Curseur 0-10 ; toText affiche la valeur réelle correspondante.
    void addSlider (Layout& layout, const char* id, const char* name, float defaultValue,
                    std::function<juce::String (float)> toText = nullptr)
    {
        if (toText == nullptr)
            toText = [] (float v) { return juce::String (v, 1); };

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 10.0f), defaultValue,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([toText] (float v, int) { return toText (v); })));
    }

    void addFloat (Layout& layout, const char* id, const char* name, juce::NormalisableRange<float> range,
                   float defaultValue, std::function<juce::String (float)> toText)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, defaultValue,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([toText] (float v, int) { return toText (v); })));
    }

    void addBool (Layout& layout, const char* id, const char* name, bool defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, defaultValue));
    }

    void addChoice (Layout& layout, const char* id, const char* name, const juce::StringArray& choices, int defaultIndex)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, defaultIndex));
    }
}

//==============================================================================
LP60Processor::LP60Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LP60", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout LP60Processor::createLayout()
{
    using namespace lp60::detail;
    Layout layout;

    auto hz = [] (float v) { return v < 10.0f ? juce::String (v, 2) + " Hz" : juce::String (v, 1) + " Hz"; };

    // Arpégiateur
    addBool (layout, "arpOn", "Arpeggio On", false);
    addChoice (layout, "arpMode", "Arpeggio Mode", { "UP", "U&D", "DOWN" }, 0);
    addChoice (layout, "arpRange", "Arpeggio Range", { "1", "2", "3" }, 0);
    addSlider (layout, "arpRate", "Arpeggio Rate", 5.0f);
    addBool (layout, "arpSync", "Arpeggio Tempo Sync", false);
    addBool (layout, "hold", "Hold", false);

    // LFO
    addSlider (layout, "lfoRate", "LFO Rate", 5.0f,
               [hz] (float v) { return hz (lookup (lfoRates, v)); });
    addSlider (layout, "lfoDelay", "LFO Delay Time", 0.0f,
               [] (float v) { return formatTime (lookup (lfoDelays, v)); });
    addChoice (layout, "lfoMode", "LFO Trigger Mode", { "AUTO", "MAN" }, 0);

    // DCO
    addChoice (layout, "range", "DCO Range", { "16'", "8'", "4'" }, 1);
    addSlider (layout, "dcoLfo", "DCO LFO", 0.0f,
               [] (float v) { return juce::String (juce::roundToInt (v * 38.0f)) + " ct"; });
    addSlider (layout, "pwm", "DCO PWM", 0.0f,
               [] (float v) { const float a = v / 10.0f; return juce::String (juce::roundToInt (49.0f - 35.0f * a - 10.0f * a * a)) + " %"; });
    addChoice (layout, "pwmMode", "DCO PWM Mode", { "LFO", "MAN", "ENV" }, 1);
    addBool (layout, "pulse", "DCO Pulse", false);
    addBool (layout, "saw", "DCO Saw", true);
    addBool (layout, "subOn", "DCO Sub On", false);
    addSlider (layout, "sub", "DCO Sub Osc", 0.0f);
    addSlider (layout, "noise", "DCO Noise", 0.0f);

    // HPF
    addChoice (layout, "hpf", "HPF Frequency", { "0", "1", "2", "3" }, 0);

    // VCF
    addSlider (layout, "cutoff", "VCF Frequency", 7.0f, [hz] (float v)
               {
                   const float f = cutoffHz (v);
                   return f < 1000.0f ? juce::String (juce::roundToInt (f)) + " Hz" : juce::String (f / 1000.0f, 2) + " kHz";
               });
    addSlider (layout, "resonance", "VCF Resonance", 0.0f);
    addChoice (layout, "envPolarity", "VCF Envelope Polarity", { "+", "-" }, 0);
    addSlider (layout, "vcfEnv", "VCF Envelope", 0.0f);
    addSlider (layout, "vcfLfo", "VCF LFO", 0.0f);
    addSlider (layout, "vcfKybd", "VCF Keyboard Follow", 0.0f,
               [] (float v) { return juce::String (juce::roundToInt (v * 10.0f)) + " %"; });

    // VCA
    addChoice (layout, "vcaMode", "VCA Mode", { "ENV", "GATE" }, 0);
    addSlider (layout, "level", "VCA Level", 7.0f);

    // ENV
    addSlider (layout, "attack", "ENV Attack", 0.0f, [] (float v) { return formatTime (lookup (attackTimes, v)); });
    addSlider (layout, "decay", "ENV Decay", 5.0f, [] (float v) { return formatTime (envDisplaySeconds (v)); });
    addSlider (layout, "sustain", "ENV Sustain", 10.0f);
    addSlider (layout, "release", "ENV Release", 3.0f, [] (float v) { return formatTime (envDisplaySeconds (v)); });

    // Chorus : les deux boutons enfoncés donnent le mode I+II.
    addBool (layout, "chorus1", "Chorus I", false);
    addBool (layout, "chorus2", "Chorus II", false);
    addSlider (layout, "chorusHiss", "Chorus Noise", 5.0f);

    // Jeu
    addFloat (layout, "tune", "Master Tune", { -50.0f, 50.0f }, 0.0f,
              [] (float v) { return juce::String (juce::roundToInt (v)) + " ct"; });
    addFloat (layout, "bendDco", "Bender DCO Range", juce::NormalisableRange<float> (0.0f, 12.0f, 1.0f), 2.0f,
              [] (float v) { return juce::String (juce::roundToInt (v)) + " st"; });
    addFloat (layout, "bendVcf", "Bender VCF", { 0.0f, 10.0f }, 0.0f, [] (float v) { return juce::String (v, 1); });
    addFloat (layout, "lfoMod", "Mod Wheel Vibrato", { 0.0f, 10.0f }, 5.0f, [] (float v) { return juce::String (v, 1); });
    addFloat (layout, "bpm", "Tempo (without host)", juce::NormalisableRange<float> (40.0f, 240.0f, 1.0f), 120.0f,
              [] (float v) { return juce::String (juce::roundToInt (v)) + " BPM"; });

    return layout;
}

lp60::Params LP60Processor::readParams() const
{
    lp60::Params p;
    p.lfoRate = param ("lfoRate");
    p.lfoDelay = param ("lfoDelay");
    p.lfoMode = choice ("lfoMode");
    p.range = choice ("range");
    p.dcoLfo = param ("dcoLfo");
    p.pwm = param ("pwm");
    p.pwmMode = choice ("pwmMode");
    p.pulse = flag ("pulse");
    p.saw = flag ("saw");
    p.sub = flag ("subOn") ? param ("sub") : 0.0f;   // bouton SUB comme sur le JU-06A
    p.noise = param ("noise");
    p.hpf = choice ("hpf");
    p.cutoff = param ("cutoff");
    p.resonance = param ("resonance");
    p.envInvert = choice ("envPolarity") == 1;
    p.vcfEnv = param ("vcfEnv");
    p.vcfLfo = param ("vcfLfo");
    p.vcfKybd = param ("vcfKybd");
    p.vcaGate = choice ("vcaMode") == 1;
    p.level = param ("level");
    p.attack = param ("attack");
    p.decay = param ("decay");
    p.sustain = param ("sustain");
    p.release = param ("release");
    p.tune = param ("tune");
    p.bendDco = param ("bendDco");
    p.bendVcf = param ("bendVcf");
    p.lfoMod = param ("lfoMod");
    return p;
}

lp60::ArpSettings LP60Processor::readArp() const
{
    lp60::ArpSettings a;
    a.on = flag ("arpOn");
    a.mode = choice ("arpMode");
    a.range = choice ("arpRange") + 1;
    a.rate = param ("arpRate");
    a.sync = flag ("arpSync");
    return a;
}

//==============================================================================
int LP60Processor::getNumPrograms()
{
    return (int) lp60::factoryPresets().size();
}

const juce::String LP60Processor::getProgramName (int index)
{
    const auto& presets = lp60::factoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? juce::String (presets[(size_t) index].name)
                                                                  : juce::String();
}

void LP60Processor::setCurrentProgram (int index)
{
    const auto& presets = lp60::factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;

    // Tout revient au réglage par défaut, sauf le jeu (accord, levier, tempo).
    static const juce::StringArray keep { "tune", "bendDco", "bendVcf", "lfoMod", "bpm", "chorusHiss" };
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! keep.contains (ranged->getParameterID()))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

//==============================================================================
void LP60Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate, samplesPerBlock);
    chorus.prepare (sampleRate);
    arpeggiator.prepare (sampleRate);
    scratch.setSize (1, samplesPerBlock);
    fromUi.ensureSize (1024);
    holdOutput.ensureSize (2048);
    uiMidi.reset (sampleRate);
    midiPlayer.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
    keyboardState.reset();
    keyDown.fill (false);
    latched.fill (false);
}

bool LP60Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

// HOLD (ou pédale de sustain) : les notes relâchées restent tenues. Une
// nouvelle note jouée après avoir tout lâché remplace l'accord tenu, comme
// sur le Juno-60.
void LP60Processor::applyHold (juce::MidiBuffer& midi)
{
    holdOutput.clear();
    const bool holding = flag ("hold") || sustainPedal;

    if (wasHolding && ! holding)
    {
        for (int n = 0; n < 128; ++n)
            if (latched[(size_t) n] && ! keyDown[(size_t) n])
                holdOutput.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        latched.fill (false);
    }

    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        const int t = metadata.samplePosition;

        if (m.isControllerOfType (64))
            sustainPedal = m.getControllerValue() >= 64;

        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            const bool noKeyDown = std::none_of (keyDown.begin(), keyDown.end(), [] (bool b) { return b; });
            if (holding && noKeyDown)
            {
                for (int other = 0; other < 128; ++other)
                    if (latched[(size_t) other])
                        holdOutput.addEvent (juce::MidiMessage::noteOff (1, other), t);
                latched.fill (false);
            }
            keyDown[(size_t) n] = true;
            if (holding)
                latched[(size_t) n] = true;
            holdOutput.addEvent (m, t);
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            keyDown[(size_t) n] = false;
            if (! holding || ! latched[(size_t) n])
                holdOutput.addEvent (m, t);
        }
        else
        {
            holdOutput.addEvent (m, t);
        }
    }

    wasHolding = flag ("hold") || sustainPedal;
    midi.swapWith (holdOutput);
}

void LP60Processor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        engine.noteOn (m.getNoteNumber());
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isControllerOfType (1))
        engine.setModWheel ((float) m.getControllerValue() / 127.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
}

void LP60Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // Le fichier MIDI joue comme le clavier (ses notes s'allument à l'écran).
    midiPlayer.process (midi, numSamples);
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
    fromUi.clear();
    uiMidi.removeNextBlockOfMessages (fromUi, numSamples);
    midi.addEvents (fromUi, 0, numSamples, 0);

    applyHold (midi);

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

    engine.setParams (readParams());

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

    // Chorus : I, II, ou les deux boutons pour I+II.
    chorus.setMode ((flag ("chorus1") ? 1 : 0) + (flag ("chorus2") ? 2 : 0));
    chorus.setHiss (param ("chorusHiss"));

    if (chorus.isOn())
    {
        if (buffer.getNumChannels() > 1)
        {
            chorus.process (out, buffer.getWritePointer (1), numSamples);
        }
        else
        {
            scratch.setSize (1, numSamples, false, false, true);
            auto* right = scratch.getWritePointer (0);
            chorus.process (out, right, numSamples);
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
juce::AudioProcessorEditor* LP60Processor::createEditor()
{
    return new LP60Editor (*this);
}

void LP60Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LP60Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LP60Processor();
}
