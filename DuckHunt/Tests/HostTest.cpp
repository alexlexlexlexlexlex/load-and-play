// Charge le plugin compilé (VST3, et AU sur Mac) comme le ferait un DAW, et vérifie
// l'entrée sidechain, le duck, la latence annoncée et la sauvegarde des réglages.
// Utilisation : HostTest <Duck Hunt.vst3> [Duck Hunt.component]

#include <juce_audio_processors/juce_audio_processors.h>

#include <iostream>

namespace
{
bool allOk = true;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "  OK    " : "  ECHEC ") << what << std::endl;
    allOk = allOk && condition;
}

juce::AudioProcessorParameter* find (juce::AudioPluginInstance& plugin, const juce::String& name)
{
    for (auto* p : plugin.getParameters())
        if (p->getName (64) == name)
            return p;
    return nullptr;
}

// Valeur de 0 à 1, comme un DAW la transmet au plugin.
void setValue (juce::AudioPluginInstance& plugin, const juce::String& name, float normalised)
{
    if (auto* p = find (plugin, name))
        p->setValueNotifyingHost (normalised);
}

float rms (const std::vector<float>& s, int start, int end)
{
    double sum = 0.0;
    for (int i = start; i < end; ++i) sum += (double) s[(size_t) i] * s[(size_t) i];
    return (float) std::sqrt (sum / (end - start));
}

void testFile (juce::AudioPluginFormat& format, const juce::String& path)
{
    std::cout << format.getName() << " : " << path << std::endl;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, path);
    check (types.size() == 1, "plugin trouvé");
    if (types.isEmpty()) return;

    const double rate = 44100.0;
    const int block = 512;
    juce::String error;
    auto plugin = format.createInstanceFromDescription (*types[0], rate, block, error);
    check (plugin != nullptr, "plugin chargé " + error.toStdString());
    if (plugin == nullptr) return;

    check (types[0]->isInstrument == false, "rangé avec les effets");
    check (plugin->acceptsMidi(), "accepte le MIDI (déclenchement par notes)");
    check (plugin->getBusCount (true) == 2, "deux entrées : la piste et le sidechain");

    auto layout = plugin->getBusesLayout();
    if (layout.inputBuses.size() == 2)
        layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    check (plugin->setBusesLayout (layout) && plugin->getBus (true, 1)->isEnabled(), "sidechain stéréo activé");

    plugin->prepareToPlay (rate, block);
    const int length = (int) (2.0 * rate);
    std::vector<float> out ((size_t) length);
    juce::AudioBuffer<float> buffer (4, block);
    juce::MidiBuffer midi;

    for (int pos = 0; pos < length; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            const int t = pos + i;
            const float main = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * t / rate);
            const float side = (t >= 22050 && t < 44100) ? 0.9f * (float) std::sin (juce::MathConstants<double>::twoPi * 60.0 * t / rate) : 0.0f;
            buffer.setSample (0, i, main);
            buffer.setSample (1, i, main);
            buffer.setSample (2, i, side);
            buffer.setSample (3, i, side);
        }
        plugin->processBlock (buffer, midi);
        for (int i = 0; i < block && pos + i < length; ++i)
            out[(size_t) (pos + i)] = buffer.getSample (0, i);
    }
    const float reduction = 20.0f * std::log10 (rms (out, 30000, 42000) / rms (out, 4410, 17640));
    check (std::abs (reduction + 12.0f) < 0.5f, "référence branchée : la piste baisse de 12 dB (" + juce::String (reduction, 2).toStdString() + " dB)");

    // Mode spectral : le plugin annonce sa latence au DAW (compensée par Cubase).
    setValue (*plugin, "Mode", 1.0f);   // dernier choix : Spectral
    plugin->processBlock (buffer, midi);
    for (int i = 0; i < 10 && plugin->getLatencySamples() == 0; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    check (plugin->getLatencySamples() == 1024, "spectral : latence annoncée " + std::to_string (plugin->getLatencySamples()) + " échantillons");

    // Sauvegarde et rappel des réglages (projet Cubase).
    setValue (*plugin, "Depth", 21.0f / 60.0f);
    juce::MemoryBlock saved;
    plugin->getStateInformation (saved);
    auto copy = format.createInstanceFromDescription (*types[0], rate, block, error);
    copy->setStateInformation (saved.getData(), (int) saved.getSize());
    auto* depth = find (*copy, "Depth");
    check (depth != nullptr && depth->getText (depth->getValue(), 32).startsWith ("21"), "réglages rappelés (Depth " + (depth ? depth->getText (depth->getValue(), 32).toStdString() : std::string ("?")) + ")");

    plugin->releaseResources();
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::cerr << "usage: HostTest <Duck Hunt.vst3> [Duck Hunt.component]" << std::endl;
        return 1;
    }

    juce::VST3PluginFormat vst3;
    testFile (vst3, argv[1]);
#if JUCE_MAC
    if (argc >= 3)
    {
        juce::AudioUnitPluginFormat au;
        juce::OwnedArray<juce::PluginDescription> found;
        au.findAllTypesForFile (found, "AudioUnit:Effects/aumf,DkHt,LdPl");
        testFile (au, "AudioUnit:Effects/aumf,DkHt,LdPl");
    }
#endif

    std::cout << (allOk ? "Tout est bon." : "Des tests ont échoué.") << std::endl;
    return allOk ? 0 : 1;
}
