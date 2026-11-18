// Outil de développement : enregistre une image PNG de l'interface du plugin,
// sans DAW ni écran. Utilisation : Snapshot <sortie.png>
// Snapshot <sortie.png> <n> : même chose avec le preset numéro n chargé.
// Snapshot --midiplayer <fichier.mid> [image.png] teste le lecteur de fichier MIDI.
// Snapshot --selftest vérifie HOLD et le chargement des presets.

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
        return midiPlayerSelfTest<LP60Processor> (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]),
                                            argc >= 4 ? juce::String (argv[3]) : juce::String());

    if (juce::String (argv[1]) == "--selftest")
    {
        LP60Processor processor;
        processor.prepareToPlay (48000.0, 256);

        auto runBlock = [&processor] (juce::MidiBuffer midi)
        {
            juce::AudioBuffer<float> audio (2, 256);
            audio.clear();
            processor.processBlock (audio, midi);
            return audio.getRMSLevel (0, 0, 256);
        };

        // HOLD : la note continue après le relâchement de la touche.
        processor.apvts.getParameter ("hold")->setValueNotifyingHost (1.0f);
        juce::MidiBuffer press, release;
        press.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        release.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        runBlock (press);
        runBlock (release);
        for (int i = 0; i < 100; ++i)
            runBlock ({});
        const bool held = runBlock ({}) > 0.001f;

        processor.apvts.getParameter ("hold")->setValueNotifyingHost (0.0f);
        for (int i = 0; i < 400; ++i)
            runBlock ({});
        const bool releasedAfterHold = runBlock ({}) < 1.0e-4f;

        // Preset « Bass » : range 16', sub au maximum ou presque.
        processor.setCurrentProgram (4);
        const bool preset = processor.getProgramName (4) == "Bass"
                         && processor.apvts.getRawParameterValue ("range")->load() < 0.5f
                         && processor.apvts.getRawParameterValue ("sub")->load() > 7.5f;

        std::cout << (held ? "  OK    " : "  ECHEC ") << "HOLD : la note reste tenue apres le relachement" << std::endl;
        std::cout << (releasedAfterHold ? "  OK    " : "  ECHEC ") << "HOLD coupe : la note s'arrete" << std::endl;
        std::cout << (preset ? "  OK    " : "  ECHEC ") << "preset Bass charge (16', sub)" << std::endl;
        return held && releasedAfterHold && preset ? 0 : 1;
    }

    LP60Processor processor;

    if (argc >= 3)
        processor.setCurrentProgram (juce::String (argv[2]).getIntValue());

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
