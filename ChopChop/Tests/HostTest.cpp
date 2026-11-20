// Charge le plugin compilé (VST3, et AU sur Mac) comme le ferait un DAW, et vérifie
// l'entrée drums (sidechain), le hachage, la latence et la sauvegarde des réglages.
// Utilisation : HostTest <Chop Chop.vst3> [Chop Chop.component]

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
    check (plugin->getBusCount (true) == 2, "deux entrées : la voix et les drums (sidechain)");

    auto layout = plugin->getBusesLayout();
    if (layout.inputBuses.size() == 2)
        layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    check (plugin->setBusesLayout (layout) && plugin->getBus (true, 1)->isEnabled(), "sidechain stéréo activé");

    check (plugin->getLatencySamples() == 0, "aucune latence");

    // Rend 2 s : une voix (bruit) et, si demandé, un coup de kick à 0,5 s sur l'entrée drums.
    auto renderOnce = [&] (bool withKick)
    {
        plugin->prepareToPlay (rate, block);
        const int length = (int) (2.0 * rate);
        std::vector<float> in ((size_t) length), out ((size_t) length);
        juce::Random random (7);
        for (auto& x : in) x = 0.3f * (random.nextFloat() * 2.0f - 1.0f);
        juce::AudioBuffer<float> buffer (4, block);
        juce::MidiBuffer midi;
        for (int pos = 0; pos + block <= length; pos += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const int t = pos + i, k = t - 22050;
                const float kick = withKick && k >= 0 && k < 3000 ? 0.9f * std::exp (-k / 882.0f) * (float) std::sin (juce::MathConstants<double>::twoPi * 60.0 * k / rate) : 0.0f;
                buffer.setSample (0, i, in[(size_t) t]);
                buffer.setSample (1, i, in[(size_t) t]);
                buffer.setSample (2, i, kick);
                buffer.setSample (3, i, kick);
            }
            plugin->processBlock (buffer, midi);
            for (int i = 0; i < block; ++i)
                out[(size_t) (pos + i)] = buffer.getSample (0, i);
        }
        float before = 0.0f, after = 0.0f;
        for (int i = 0; i < 22000; ++i) before = std::max (before, std::abs (out[(size_t) i] - in[(size_t) i]));
        for (int i = 22200; i < 30000; ++i) after = std::max (after, std::abs (out[(size_t) i] - in[(size_t) i]));
        return std::make_pair (before, after);
    };

    const auto plain = renderOnce (true);
    check (plain.first == 0.0f && plain.second == 0.0f, "réglages par défaut : la voix passe telle quelle");

    setValue (*plugin, "Trigger", 0.5f);   // DRUMS
    const auto chopped = renderOnce (true);
    check (chopped.first == 0.0f && chopped.second > 0.05f, "mode DRUMS : la voix est hachée sur le coup de kick, intacte avant");

    // Sauvegarde et rappel des réglages (projet Cubase).
    setValue (*plugin, "Drive", 21.0f / 40.0f);
    juce::MemoryBlock saved;
    plugin->getStateInformation (saved);
    auto copy = format.createInstanceFromDescription (*types[0], rate, block, error);
    copy->setStateInformation (saved.getData(), (int) saved.getSize());
    auto* drive = find (*copy, "Drive");
    check (drive != nullptr && drive->getText (drive->getValue(), 32).startsWith ("21"),
           "réglages rappelés (Drive " + (drive ? drive->getText (drive->getValue(), 32).toStdString() : std::string ("?")) + ")");

    plugin->releaseResources();
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::cerr << "usage: HostTest <Chop Chop.vst3> [Chop Chop.component]" << std::endl;
        return 1;
    }

    juce::VST3PluginFormat vst3;
    testFile (vst3, argv[1]);
#if JUCE_MAC
    if (argc >= 3)
    {
        juce::AudioUnitPluginFormat au;
        juce::OwnedArray<juce::PluginDescription> found;
        au.findAllTypesForFile (found, "AudioUnit:Effects/aumf,ChCh,LdPl");
        testFile (au, "AudioUnit:Effects/aumf,ChCh,LdPl");
    }
#endif

    std::cout << (allOk ? "Tout est bon." : "Des tests ont échoué.") << std::endl;
    return allOk ? 0 : 1;
}
