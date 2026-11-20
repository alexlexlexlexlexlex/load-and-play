#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace chopchop;

namespace
{
juce::String formatMs (float v, int) { return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " ms" : juce::String (v / 1000.0f, 2) + " s"; }
juce::String formatHz (float v, int) { return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz"; }
juce::String formatDb (float v, int) { return juce::String (v, 1) + " dB"; }
juce::String formatPercent (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }
juce::String formatSemitones (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (std::abs (v) < 0.05f ? 0.0f : v, 1) + " st"; }

juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> range (lo, hi);
    range.setSkewForCentre (centre);
    return range;
}

juce::StringArray names (const char* const* list, int count)
{
    juce::StringArray result;
    for (int i = 0; i < count; ++i) result.add (list[i]);
    return result;
}

// Presets : un motif (- passe, R répète, L roll, < envers, U +12, D -12, X coupe)
// et des valeurs posées sur les réglages par défaut.
struct Preset
{
    const char* name;
    const char* pattern;
    std::vector<std::pair<const char*, float>> values;
};

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        { "Init", "----------------", {} },
        { "Bégaiement de fin de mesure", "------------LLLL", { { "rise", 1.0f }, { "drive", 6.0f }, { "echo", 20.0f },
                                                               { "delayDivision", 2.0f }, { "reverb", 15.0f } } },
        { "Voix hachée sur les drums", "RRURRR<RRRURRRLL", { { "trigger", 1.0f }, { "band", 2.0f }, { "hitLength", 2.0f },
                                                             { "dirt", 1.0f }, { "drive", 10.0f }, { "drumDrive", 12.0f }, { "pump", 6.0f } } },
        { "Le kick découpe", "RU<RRD<RRU<RRDLL", { { "trigger", 1.0f }, { "band", 0.0f }, { "hitLength", 1.0f }, { "size", 3.0f },
                                                   { "drive", 12.0f }, { "drumDrive", 8.0f }, { "pump", 9.0f } } },
        { "Chipmunk rave", "U-U-UU--U-U-UULL", { { "drive", 14.0f }, { "drumDrive", 6.0f }, { "pump", 8.0f }, { "reverb", 20.0f } } },
        { "Téléphone sale", "R---R---R--RR-LL", { { "dirt", 2.0f }, { "drive", 20.0f }, { "filter", 3.0f }, { "cutoff", 1500.0f },
                                                  { "resonance", 1.5f }, { "echo", 30.0f }, { "delayDivision", 1.0f }, { "feedback", 45.0f } } },
        { "Rembobinage", "----<<<<----DDDD", { { "size", 1.0f }, { "drive", 8.0f }, { "reverb", 25.0f } } },
        { "Pluie de répétitions", "--------LLLLLLLL", { { "size", 1.0f }, { "rise", 1.0f }, { "decay", 92.0f }, { "drive", 10.0f },
                                                       { "filter", 2.0f }, { "cutoff", 250.0f } } },
        { "Gate hachée", "-X-X-X-X-X-X-XXX", { { "dirt", 1.0f }, { "drive", 6.0f }, { "pump", 6.0f } } },
        { "Distorsion qui respire", "----R-------R---", { { "dirt", 1.0f }, { "drive", 6.0f }, { "drumDrive", 18.0f }, { "filter", 1.0f },
                                                         { "cutoff", 2500.0f }, { "filterMod", 60.0f }, { "pump", 10.0f } } },
        { "Voix au clavier (MIDI)", "----------------", { { "trigger", 2.0f }, { "size", 1.0f }, { "drive", 6.0f }, { "echo", 25.0f },
                                                          { "delayDivision", 2.0f }, { "reverb", 20.0f } } },
    };
    return list;
}
} // namespace

ChopChopProcessor::ChopChopProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Drums", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "ChopChop", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ChopChopProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const int version = 1;

    auto addFloat = [&] (const String& id, const String& name, NormalisableRange<float> range, float def, String (*format) (float, int))
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, version }, name, range, def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (format)));
    };
    auto addChoice = [&] (const String& id, const String& name, StringArray choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, version }, name, choices, def));
    };

    addChoice ("trigger", "Trigger", { "Pattern", "Drums", "MIDI" }, 0);
    addChoice ("stepRate", "Step Rate", names (stepRateNames, 3), 1);
    for (int i = 0; i < numSteps; ++i)
        addChoice (stepId (i), "Step " + String (i + 1), names (stepNames, numStepTypes), 0);

    addChoice ("size", "Slice Size", names (sizeNames, 5), 2);
    addFloat ("rise", "Rise", { -12.0f, 12.0f, 0.1f }, 0.0f, formatSemitones);
    addFloat ("decay", "Decay", { 0.0f, 100.0f, 1.0f }, 100.0f, formatPercent);
    addFloat ("chance", "Chance", { 0.0f, 100.0f, 1.0f }, 100.0f, formatPercent);
    addFloat ("pitch", "Pitch", { -12.0f, 12.0f, 0.1f }, 0.0f, formatSemitones);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "snap", version }, "Snap To Syllable", true));

    addChoice ("band", "Drum Band", { "Kick", "Snare", "All" }, 0);
    addFloat ("sensitivity", "Sensitivity", { -60.0f, 0.0f, 0.1f }, -30.0f, formatDb);
    addChoice ("hitLength", "Hit Length", names (hitNames, 5), 2);

    addChoice ("dirt", "Dirt", { "Warm", "Fuzz", "Crush" }, 0);
    addFloat ("drive", "Drive", { 0.0f, 40.0f, 0.1f }, 0.0f, formatDb);
    addFloat ("drumDrive", "Drum Drive", { 0.0f, 24.0f, 0.1f }, 0.0f, formatDb);

    addChoice ("filter", "Filter", { "Off", "Low-pass", "High-pass", "Band-pass" }, 0);
    addFloat ("cutoff", "Cutoff", skewed (40.0f, 18000.0f, 1000.0f), 1000.0f, formatHz);
    addFloat ("resonance", "Resonance", skewed (0.5f, 10.0f, 1.5f), 0.7f, [] (float v, int) { return juce::String (v, 2); });
    addFloat ("filterMod", "Drum Filter Mod", { -100.0f, 100.0f, 1.0f }, 0.0f, formatPercent);

    addFloat ("pump", "Pump", { 0.0f, 24.0f, 0.1f }, 0.0f, formatDb);
    addFloat ("pumpRelease", "Pump Release", skewed (20.0f, 1000.0f, 150.0f), 150.0f, formatMs);

    addChoice ("delayDivision", "Delay Time", names (delayNames, 6), 2);
    addFloat ("feedback", "Feedback", { 0.0f, 90.0f, 1.0f }, 35.0f, formatPercent);
    addFloat ("echo", "Echo", { 0.0f, 100.0f, 1.0f }, 0.0f, formatPercent);
    addFloat ("reverbSize", "Reverb Size", { 0.0f, 100.0f, 1.0f }, 50.0f, formatPercent);
    addFloat ("reverb", "Reverb", { 0.0f, 100.0f, 1.0f }, 0.0f, formatPercent);

    addFloat ("mix", "Mix", { 0.0f, 100.0f, 1.0f }, 100.0f, formatPercent);
    addFloat ("output", "Output", { -24.0f, 12.0f, 0.1f }, 0.0f, formatDb);
    return layout;
}

Params ChopChopProcessor::readParams() const
{
    auto get = [this] (const juce::String& id) { return state.getRawParameterValue (id)->load(); };
    auto index = [&] (const juce::String& id) { return juce::roundToInt (get (id)); };

    Params p;
    p.trigger = (Trigger) index ("trigger");
    p.stepRate = index ("stepRate");
    for (int i = 0; i < numSteps; ++i)
        p.pattern[(size_t) i] = (Step) index (stepId (i));
    p.size = index ("size");
    p.rise = get ("rise");
    p.decay = get ("decay") * 0.01f;
    p.chance = get ("chance") * 0.01f;
    p.pitch = get ("pitch");
    p.snap = get ("snap") > 0.5f;
    p.band = (DrumBand) index ("band");
    p.sensitivityDb = get ("sensitivity");
    p.hitLength = index ("hitLength");
    p.dirt = (Dirt) index ("dirt");
    p.driveDb = get ("drive");
    p.drumDriveDb = get ("drumDrive");
    p.filter = (FilterType) index ("filter");
    p.cutoff = get ("cutoff");
    p.resonance = get ("resonance");
    p.filterMod = get ("filterMod") * 0.01f;
    p.pumpDb = get ("pump");
    p.pumpReleaseMs = get ("pumpRelease");
    p.delayDivision = index ("delayDivision");
    p.feedback = get ("feedback") * 0.01f;
    p.echo = get ("echo") * 0.01f;
    p.reverbSize = get ("reverbSize") * 0.01f;
    p.reverb = get ("reverb") * 0.01f;
    p.mix = get ("mix") * 0.01f;
    p.outputDb = get ("output");
    return p;
}

//==============================================================================
void ChopChopProcessor::prepareToPlay (double newRate, int)
{
    sampleRate = newRate;
    engine.prepare (newRate);
    engine.setParams (readParams());
}

bool ChopChopProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out || (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()))
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto side = layouts.getChannelSet (true, 1);
        if (! side.isDisabled() && side != juce::AudioChannelSet::mono() && side != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void ChopChopProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    auto mainBuffer = getBusBuffer (buffer, true, 0);
    const int numMain = std::min (2, mainBuffer.getNumChannels());

    const float* side[2] { nullptr, nullptr };
    int numSide = 0;
    if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0)
    {
        auto sideBuffer = getBusBuffer (buffer, true, 1);
        numSide = std::min (2, sideBuffer.getNumChannels());
        for (int ch = 0; ch < numSide; ++ch)
            side[ch] = sideBuffer.getReadPointer (ch);
    }
    sidechainActive = numSide > 0;

    engine.setParams (readParams());

    Transport transport;
    if (auto* head = getPlayHead())
        if (const auto position = head->getPosition())
        {
            transport.bpm = position->getBpm().orFallback (120.0);
            transport.ppq = position->getPpqPosition().orFallback (0.0);
            transport.playing = position->getIsPlaying();
        }

    // Découpage du bloc à chaque note MIDI, pour un déclenchement à l'échantillon près.
    float* channels[2] { nullptr, nullptr };
    const float* sides[2] { nullptr, nullptr };
    int position = 0;

    auto run = [&] (int end)
    {
        if (end <= position) return;
        for (int ch = 0; ch < numMain; ++ch) channels[ch] = mainBuffer.getWritePointer (ch) + position;
        for (int ch = 0; ch < numSide; ++ch) sides[ch] = side[ch] + position;
        Transport t = transport;
        t.ppq += position * transport.bpm / 60.0 / sampleRate;
        engine.process (channels, numMain, sides, numSide, end - position, t);
        position = end;
    };

    for (const auto event : midi)
    {
        const auto message = event.getMessage();
        if (! message.isNoteOnOrOff()) continue;
        run (std::clamp (event.samplePosition, 0, numSamples));
        if (message.isNoteOn()) engine.noteOn (message.getNoteNumber(), message.getFloatVelocity());
        else engine.noteOff (message.getNoteNumber());
    }
    run (numSamples);

    for (int ch = numMain; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

//==============================================================================
int ChopChopProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String ChopChopProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? juce::String::fromUTF8 (presets()[(size_t) index].name) : juce::String();
}

void ChopChopProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    currentProgram = index;
    const auto& preset = presets()[(size_t) index];
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());

    const auto steps = patternFromString (preset.pattern);
    for (int i = 0; i < numSteps; ++i)
        if (auto* param = state.getParameter (stepId (i)))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) steps[(size_t) i]));

    for (const auto& [id, value] : preset.values)
        if (auto* param = state.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void ChopChopProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("program", currentProgram, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, dest);
}

void ChopChopProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (state.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            state.replaceState (tree);
        }
}

juce::AudioProcessorEditor* ChopChopProcessor::createEditor() { return new ChopChopEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChopChopProcessor(); }
