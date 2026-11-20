#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
// Prints one JSON line per parameter of a VST3: id, name, min, max, default, choices.
int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile (found, argv[1]);
    if (found.isEmpty()) { std::cerr << "not found\n"; return 1; }
    juce::String error;
    auto plugin = format.createInstanceFromDescription (*found[0], 48000.0, 512, error);
    if (! plugin) { std::cerr << error << "\n"; return 1; }
    juce::Array<juce::var> out;
    for (auto* p : plugin->getParameters())
    {
        auto* o = new juce::DynamicObject();
        if (auto* h = dynamic_cast<juce::HostedAudioProcessorParameter*> (p)) o->setProperty ("id", h->getParameterID());
        o->setProperty ("name", p->getName (100));
        o->setProperty ("label", p->getLabel());
        o->setProperty ("min", p->getText (0.0f, 100));
        o->setProperty ("max", p->getText (1.0f, 100));
        o->setProperty ("def", p->getText (p->getDefaultValue(), 100));
        o->setProperty ("steps", p->getNumSteps());
        if (p->isDiscrete() && p->getNumSteps() <= 24)
        {
            juce::Array<juce::var> c;
            for (int i = 0; i < p->getNumSteps(); ++i)
                c.add (p->getText ((float) i / (float) juce::jmax (1, p->getNumSteps() - 1), 100));
            o->setProperty ("choices", c);
        }
        out.add (juce::var (o));
    }
    for (int i = 0; i < plugin->getNumPrograms(); ++i) std::cerr << "PROGRAM " << plugin->getProgramName (i) << "\n";
    std::cout << juce::JSON::toString (juce::var (out), true) << std::endl;
    return 0;
}
