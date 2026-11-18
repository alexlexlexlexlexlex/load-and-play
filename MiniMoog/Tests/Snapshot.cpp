// Outil de développement : enregistre une image PNG de l'interface du plugin,
// sans DAW ni écran. Utilisation : Snapshot <sortie.png>
// Snapshot <sortie.png> demo : même chose avec un son réglé (pour les captures du site).
// Snapshot --midiplayer <fichier.mid> [image.png] teste le lecteur de fichier MIDI.
// Snapshot --selftest vérifie le MIDI learn et sa sauvegarde dans le projet.

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

// Lecteur MIDI : charge un fichier, joue, met en pause, va jusqu'au bout sans
// boucle. Avec un chemin d'image en plus, enregistre l'interface avec le lecteur.
template <typename Processor>
int midiPlayerSelfTest (const juce::File& midiFile, const juce::String& imagePath)
{
    Processor processor;
    processor.prepareToPlay (48000.0, 256);
    auto& player = processor.midiPlayer;

    juce::AudioBuffer<float> audio (2, 256);
    auto runBlock = [&]
    {
        juce::MidiBuffer midi;
        audio.clear();
        processor.processBlock (audio, midi);
        return audio.getRMSLevel (0, 0, 256);
    };
    auto anyKeyDown = [&]
    {
        for (int n = 0; n < 128; ++n)
            if (processor.keyboardState.isNoteOn (1, n))
                return true;
        return false;
    };

    const bool hiddenAtStart = ! player.isLoaded();
    const bool loaded = player.load (midiFile);

    player.setPlaying (true);
    float loudest = 0.0f;
    bool keysLit = false;
    for (int i = 0; i < 48000 / 256; ++i)              // 1 s
    {
        loudest = std::max (loudest, runBlock());
        keysLit = keysLit || anyKeyDown();
    }

    player.setPlaying (false);
    for (int i = 0; i < 3 * 48000 / 256; ++i)          // 3 s de pause
        runBlock();
    const bool silentWhenPaused = runBlock() < 1.0e-4f && ! anyKeyDown();

    player.setLooping (false);
    player.restart();
    player.setPlaying (true);
    for (int i = 0; i < 40 * 48000 / 256 && player.isPlaying(); ++i)
        runBlock();
    const bool stopsAtEnd = ! player.isPlaying();

    bool ok = true;
    auto check = [&ok] (bool condition, const char* what)
    {
        std::cout << (condition ? "  OK    " : "  ECHEC ") << what << std::endl;
        ok = ok && condition;
    };
    check (hiddenAtStart, "lecteur MIDI absent tant qu'aucun fichier n'est glisse");
    check (loaded, "fichier MIDI charge");
    check (loudest > 0.01f && keysLit, "lecture : le synthe joue et les touches s'allument");
    check (silentWhenPaused, "pause : les notes s'arretent");
    check (stopsAtEnd, "sans boucle : la lecture s'arrete a la fin du fichier");

    if (imagePath.isNotEmpty())
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        if (auto* constrainer = editor->getConstrainer())
            editor->setSize (constrainer->getMaximumWidth() / 2, constrainer->getMaximumHeight() / 2);
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (imagePath));
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);
    }
    return ok ? 0 : 1;
}

int main (int argc, char* argv[])
{
    if (argc < 2)
        return 2;

    juce::ScopedJuceInitialiser_GUI gui;

    if (juce::String (argv[1]) == "--midiplayer" && argc >= 3)
        return midiPlayerSelfTest<ModelDProcessor> (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]),
                                            argc >= 4 ? juce::String (argv[3]) : juce::String());

    if (juce::String (argv[1]) == "--selftest")
    {
        ModelDProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        processor.setLearnTarget ("cutoff");

        juce::AudioBuffer<float> audio (2, 256);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 21, 100), 10);
        processor.processBlock (audio, midi);

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        ModelDProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());

        const bool learned = processor.getCcFor ("cutoff") == 21;
        const bool saved = restored.getCcFor ("cutoff") == 21;
        const bool equalChannels = audio.getMagnitude (0, 0, 256) == audio.getMagnitude (1, 0, 256);
        std::cout << (learned ? "  OK    " : "  ECHEC ") << "MIDI learn : CC 21 associe a Cutoff" << std::endl;
        std::cout << (saved ? "  OK    " : "  ECHEC ") << "association retrouvee apres sauvegarde du projet" << std::endl;
        std::cout << (equalChannels ? "  OK    " : "  ECHEC ") << "effets coupes : gauche = droite" << std::endl;
        return learned && saved && equalChannels ? 0 : 1;
    }

    ModelDProcessor processor;

    if (argc >= 3 && juce::String (argv[2]) == "demo")
    {
        const std::pair<const char*, float> demo[] = {
            { "cutoff", 900.0f }, { "emphasis", 4.5f }, { "contour", 6.5f }, { "glide", 0.08f },
            { "osc3On", 1.0f }, { "osc3Level", 6.0f }, { "osc3Range", 2.0f }, { "osc1Range", 2.0f },
            { "kbd1", 1.0f }, { "fDecay", 0.6f }, { "fSustain", 2.0f },
            { "chorusMode", 1.0f }, { "delayOn", 1.0f }, { "delaySync", 1.0f }, { "delayLevel", 3.5f },
            { "lfoLevel", 2.5f }, { "lfoDest", 2.0f }, { "lfoSync", 1.0f },
            { "arpOn", 1.0f }, { "arpRange", 1.0f }, { "arpGate", 1.0f }, { "arpMode", 2.0f },
            { "arpStep2", 0.0f }, { "arpStep5", 0.0f }, { "arpStep7", 0.0f }, { "arpStep10", 0.0f },
            { "arpStep13", 0.0f }, { "arpStep14", 0.0f },
        };
        for (const auto& [id, value] : demo)
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    // Taille de référence de l'interface, quelle que soit la taille de l'écran.
    if (auto* constrainer = editor->getConstrainer())
        editor->setSize (constrainer->getMaximumWidth() / 2, constrainer->getMaximumHeight() / 2);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    file.deleteFile();

    juce::FileOutputStream stream (file);
    juce::PNGImageFormat png;
    return png.writeImageToStream (image, stream) ? 0 : 1;
}
