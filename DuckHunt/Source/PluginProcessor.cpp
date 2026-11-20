#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace duckhunt;

namespace
{
juce::String formatMs (float v, int) { return v < 0.005f ? juce::String ("0 ms") : v < 1.0f ? juce::String (v, 2) + " ms" : (v < 1000.0f ? juce::String (juce::roundToInt (v)) + " ms" : juce::String (v / 1000.0f, 2) + " s"); }
juce::String formatHz (float v, int) { return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz"; }
juce::String formatDb (float v, int) { return juce::String (v, 1) + " dB"; }
juce::String formatPercent (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }

juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> range (lo, hi);
    range.setSkewForCentre (centre);
    return range;
}

// Presets : valeurs posées sur les réglages par défaut.
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        { "Init", {} },
        { "Basse sous le kick", { { "depth", 15.0f }, { "threshold", -32.0f }, { "range", 10.0f }, { "attack", 2.0f },
                                  { "hold", 15.0f }, { "release", 160.0f }, { "curve", 0.3f }, { "scHighCut", 200.0f } } },
        { "Kick : graves seulement", { { "mode", 1.0f }, { "depth", 18.0f }, { "threshold", -34.0f }, { "attack", 2.0f },
                                       { "release", 180.0f }, { "crossoverLow", 160.0f }, { "bandMid", 25.0f }, { "bandHigh", 0.0f } } },
        { "Place pour la voix", { { "mode", 2.0f }, { "depth", 6.0f }, { "threshold", -45.0f }, { "range", 20.0f },
                                  { "attack", 10.0f }, { "hold", 30.0f }, { "release", 250.0f }, { "focusLow", 200.0f },
                                  { "focusHigh", 8000.0f }, { "smoothing", 60.0f } } },
        { "Voix sur la musique (radio)", { { "depth", 9.0f }, { "threshold", -40.0f }, { "range", 15.0f }, { "attack", 40.0f },
                                           { "hold", 300.0f }, { "release", 900.0f }, { "lookahead", 20.0f }, { "scLowCut", 150.0f } } },
        { "Pompe 1/4", { { "trigger", 2.0f }, { "depth", 30.0f }, { "attack", 1.0f }, { "hold", 0.0f }, { "release", 380.0f },
                         { "curve", 0.5f } } },
        { "Pompe 1/8 graves", { { "trigger", 2.0f }, { "mode", 1.0f }, { "division", 3.0f }, { "depth", 24.0f }, { "attack", 1.0f },
                                { "hold", 0.0f }, { "release", 200.0f }, { "bandMid", 40.0f }, { "bandHigh", 0.0f } } },
        // Le pompage house : sur chaque noire, les graves plongent, les médiums un peu
        // moins, les aigus juste assez pour respirer ; le son reste bas puis regonfle
        // jusqu'au coup suivant (relâchement calé pour 120 à 128 BPM).
        { "House Rave", { { "trigger", 2.0f }, { "mode", 1.0f }, { "division", 2.0f }, { "depth", 22.0f },
                          { "attack", 1.0f }, { "hold", 25.0f }, { "release", 330.0f }, { "curve", 0.35f },
                          { "crossoverLow", 150.0f }, { "crossoverHigh", 3000.0f },
                          { "bandMid", 80.0f }, { "bandHigh", 45.0f } } },
        { "Hachures MIDI", { { "trigger", 1.0f }, { "depth", 40.0f }, { "attack", 1.0f }, { "hold", 60.0f }, { "release", 90.0f } } },
        { "Gate par la référence", { { "invert", 1.0f }, { "depth", 40.0f }, { "threshold", -40.0f }, { "range", 6.0f },
                                     { "attack", 1.0f }, { "hold", 40.0f }, { "release", 120.0f } } },
    };
    return list;
}
} // namespace

DuckHuntProcessor::DuckHuntProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "DuckHunt", createLayout())
{
    startTimerHz (5);
}

DuckHuntProcessor::~DuckHuntProcessor() { stopTimer(); }

juce::AudioProcessorValueTreeState::ParameterLayout DuckHuntProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    int version = 1;

    auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> range, float def,
                         String (*format) (float, int), const char* unit = "")
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, version }, name, range, def,
                                                           AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (format)));
    };
    auto addChoice = [&] (const char* id, const char* name, StringArray choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, version }, name, choices, def));
    };
    auto addBool = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, version }, name, def));
    };

    StringArray divisions;
    for (auto* d : divisionNames) divisions.add (d);

    addChoice ("trigger", "Trigger", { "Sidechain", "MIDI", "Tempo" }, 0);
    addChoice ("mode", "Mode", { "Full band", "3 bands", "Spectral" }, 0);
    addFloat ("depth", "Depth", { 0.0f, 60.0f, 0.1f }, 12.0f, formatDb);
    addFloat ("threshold", "Threshold", { -60.0f, 0.0f, 0.1f }, -30.0f, formatDb);
    addFloat ("range", "Range", { 1.0f, 40.0f, 0.1f }, 12.0f, formatDb);
    addFloat ("attack", "Attack", skewed (0.1f, 500.0f, 15.0f), 5.0f, formatMs);
    addFloat ("hold", "Hold", skewed (0.0f, 1000.0f, 80.0f), 20.0f, formatMs);
    addFloat ("release", "Release", skewed (5.0f, 3000.0f, 250.0f), 200.0f, formatMs);
    addFloat ("curve", "Curve", { -1.0f, 1.0f, 0.01f }, 0.0f, [] (float v, int) { return juce::String (std::abs (v) < 0.005f ? 0.0f : v, 2); });
    addFloat ("lookahead", "Lookahead", { 0.0f, maxLookaheadMs, 0.1f }, 0.0f, formatMs);
    addChoice ("division", "Division", divisions, 2);
    addBool ("velocity", "Velocity", true);
    addFloat ("scLowCut", "SC Low Cut", skewed (20.0f, 2000.0f, 150.0f), 20.0f, formatHz);
    addFloat ("scHighCut", "SC High Cut", skewed (200.0f, 20000.0f, 3000.0f), 20000.0f, formatHz);
    addFloat ("crossoverLow", "Low Crossover", skewed (40.0f, 1000.0f, 200.0f), 200.0f, formatHz);
    addFloat ("crossoverHigh", "High Crossover", skewed (800.0f, 12000.0f, 2500.0f), 2500.0f, formatHz);
    addFloat ("bandLow", "Low Band", { 0.0f, 100.0f, 1.0f }, 100.0f, formatPercent);
    addFloat ("bandMid", "Mid Band", { 0.0f, 100.0f, 1.0f }, 100.0f, formatPercent);
    addFloat ("bandHigh", "High Band", { 0.0f, 100.0f, 1.0f }, 100.0f, formatPercent);
    addFloat ("focusLow", "Focus Low", skewed (20.0f, 20000.0f, 640.0f), 20.0f, formatHz);
    addFloat ("focusHigh", "Focus High", skewed (20.0f, 20000.0f, 640.0f), 20000.0f, formatHz);
    addFloat ("smoothing", "Smoothing", { 0.0f, 100.0f, 1.0f }, 50.0f, formatPercent);
    addFloat ("mix", "Mix", { 0.0f, 100.0f, 1.0f }, 100.0f, formatPercent);
    addFloat ("output", "Output", { -24.0f, 24.0f, 0.1f }, 0.0f, formatDb);
    addBool ("invert", "Invert", false);
    addBool ("listen", "Listen Sidechain", false);
    return layout;
}

Params DuckHuntProcessor::readParams() const
{
    auto get = [this] (const char* id) { return state.getRawParameterValue (id)->load(); };
    Params p;
    p.trigger = (Trigger) juce::roundToInt (get ("trigger"));
    p.mode = (Mode) juce::roundToInt (get ("mode"));
    p.depthDb = get ("depth");
    p.thresholdDb = get ("threshold");
    p.rangeDb = get ("range");
    p.attackMs = get ("attack");
    p.holdMs = get ("hold");
    p.releaseMs = get ("release");
    p.curve = get ("curve");
    p.lookaheadMs = get ("lookahead");
    p.division = juce::roundToInt (get ("division"));
    p.velocity = get ("velocity") > 0.5f;
    p.scLowCut = get ("scLowCut");
    p.scHighCut = get ("scHighCut");
    p.crossoverLow = get ("crossoverLow");
    p.crossoverHigh = get ("crossoverHigh");
    p.bandAmount = { get ("bandLow") * 0.01f, get ("bandMid") * 0.01f, get ("bandHigh") * 0.01f };
    p.focusLow = get ("focusLow");
    p.focusHigh = get ("focusHigh");
    p.smoothing = get ("smoothing") * 0.01f;
    p.mix = get ("mix") * 0.01f;
    p.outputDb = get ("output");
    p.invert = get ("invert") > 0.5f;
    p.listen = get ("listen") > 0.5f;
    return p;
}

//==============================================================================
void DuckHuntProcessor::prepareToPlay (double newRate, int)
{
    sampleRate = newRate;
    engine.prepare (newRate);
    const auto p = readParams();
    engine.setParams (p);
    wantedLatency = DuckEngine::latencyFor (p, newRate, engine.spectralSize());
    setLatencySamples (wantedLatency);
}

bool DuckHuntProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void DuckHuntProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    process (buffer, midi, false);
}

void DuckHuntProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Contourné : le signal sec passe, avec la même latence (le DAW reste calé).
    process (buffer, midi, true);
}

void DuckHuntProcessor::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool bypassed)
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

    const auto p = readParams();
    engine.setParams (p);
    wantedLatency = DuckEngine::latencyFor (p, sampleRate, engine.spectralSize());

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
        engine.process (channels, numMain, sides, numSide, end - position, t, bypassed);
        position = end;
    };

    if (! bypassed)
        for (const auto event : midi)
            if (event.getMessage().isNoteOn())
            {
                run (std::clamp (event.samplePosition, 0, numSamples));
                engine.noteOn (event.getMessage().getFloatVelocity());
            }
    run (numSamples);

    for (int ch = numMain; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

void DuckHuntProcessor::timerCallback()
{
    // La latence change avec l'anticipation et le mode spectral : on prévient le DAW.
    if (wantedLatency.load() != getLatencySamples())
        setLatencySamples (wantedLatency.load());
}

//==============================================================================
int DuckHuntProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String DuckHuntProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? juce::String::fromUTF8 (presets()[(size_t) index].name) : juce::String();
}

void DuckHuntProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    currentProgram = index;
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());

    for (const auto& [id, value] : presets()[(size_t) index].values)
        if (auto* param = state.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void DuckHuntProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("program", currentProgram, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, dest);
}

void DuckHuntProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (state.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            state.replaceState (tree);
        }
}

juce::AudioProcessorEditor* DuckHuntProcessor::createEditor() { return new DuckHuntEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DuckHuntProcessor(); }
