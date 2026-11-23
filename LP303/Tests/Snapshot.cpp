// Outil de développement : enregistre une image PNG de l'interface du plugin,
// sans DAW ni écran. Utilisation : Snapshot <sortie.png> [n° de preset] [compact]
// (compact : séquenceur replié).
// Snapshot --selftest vérifie les presets, la sauvegarde des motifs et le
// séquenceur sur l'horloge interne (application autonome).

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

namespace
{
    int selfTest()
    {
        bool ok = true;
        auto check = [&ok] (bool condition, const char* what)
        {
            std::cout << (condition ? "  OK    " : "  ECHEC ") << what << std::endl;
            ok = ok && condition;
        };

        LP303Processor processor;
        processor.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> audio (2, 256);
        auto runBlock = [&] (juce::MidiBuffer midi)
        {
            audio.clear();
            processor.processBlock (audio, midi);
            return audio.getRMSLevel (0, 0, 256);
        };

        // Preset « Screamer » : saturation et résonance hautes, le séquenceur ne bouge pas.
        processor.apvts.getParameter ("pattern")->setValueNotifyingHost (processor.apvts.getParameter ("pattern")->convertTo0to1 (2.0f));
        processor.setCurrentProgram (4);
        check (processor.getProgramName (4) == "Screamer"
                   && processor.apvts.getRawParameterValue ("drive")->load() > 5.5f
                   && juce::roundToInt (processor.apvts.getRawParameterValue ("pattern")->load()) == 2,
               "preset Screamer charge, motif choisi conserve");

        // Séquenceur sur l'horloge interne : du son, et le pas avance.
        processor.apvts.getParameter ("seqOn")->setValueNotifyingHost (1.0f);
        float loudest = 0.0f;
        for (int i = 0; i < 48000 / 256; ++i)
            loudest = juce::jmax (loudest, runBlock ({}));
        check (loudest > 0.01f && processor.isSequencerRunning() && processor.getPlayingStep() >= 0,
               "sequenceur sur l'horloge interne : il joue");

        // Une touche transpose le motif pendant la lecture.
        juce::MidiBuffer key;
        key.addEvent (juce::MidiMessage::noteOn (1, 63, (juce::uint8) 100), 0);
        runBlock (key);
        processor.flushPendingChanges();
        check (juce::roundToInt (processor.apvts.getRawParameterValue ("transpose")->load()) == 3,
               "touche D#3 pendant la lecture : transposition +3");

        processor.apvts.getParameter ("seqOn")->setValueNotifyingHost (0.0f);
        for (int i = 0; i < 40; ++i)
            runBlock ({});
        check (runBlock ({}) < 1.0e-4f, "arret : silence");

        // Fichier MIDI glissé sur la fenêtre : il joue le synthé comme le clavier.
        {
            juce::MidiMessageSequence track;
            track.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0.0);
            track.addEvent (juce::MidiMessage::noteOff (1, 48), 480.0);
            juce::MidiFile file;
            file.setTicksPerQuarterNote (480);
            file.addTrack (track);
            const auto temp = juce::File::createTempFile (".mid");
            {
                juce::FileOutputStream out (temp);
                file.writeTo (out);
            }
            const bool loaded = processor.midiPlayer.load (temp);
            processor.midiPlayer.setPlaying (true);
            float level = 0.0f;
            for (int i = 0; i < 48000 / 2 / 256; ++i)
                level = juce::jmax (level, runBlock ({}));
            processor.midiPlayer.setPlaying (false);
            for (int i = 0; i < 40; ++i)
                runBlock ({});
            temp.deleteFile();
            check (loaded && level > 0.01f && runBlock ({}) < 1.0e-4f, "fichier MIDI : il joue, puis se tait a la pause");
        }

        // Sauvegarde et relecture, motifs compris.
        lp303::Step st;
        st.note = 11;
        st.accent = true;
        processor.patterns.set (5, 7, st);
        processor.patterns.setLength (5, 9);
        processor.sequencerShown = false;
        juce::MemoryBlock state;
        processor.getStateInformation (state);

        LP303Processor other;
        other.setStateInformation (state.getData(), (int) state.getSize());
        check (other.patterns.get (5, 7) == st && other.patterns.getLength (5) == 9
                   && other.apvts.getRawParameterValue ("drive")->load() > 5.5f && ! other.sequencerShown.load(),
               "projet sauvegarde puis rouvert : reglages, motifs et sequenceur replie identiques");

        // Interface : replier le séquenceur réduit la hauteur, le rouvrir la rend.
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            auto* lp = dynamic_cast<LP303Editor*> (editor.get());
            const int tall = editor->getHeight(), wide = editor->getWidth();
            lp->showSequencer (true);
            const int tallAgain = editor->getHeight();
            lp->showSequencer (false);
            check (tall < tallAgain && editor->getWidth() == wide && editor->getHeight() == tall,
                   "sequenceur replie : fenetre plus basse, meme largeur");
        }

        return ok ? 0 : 1;
    }
}

int main (int argc, char* argv[])
{
    if (argc < 2)
        return 2;

    juce::ScopedJuceInitialiser_GUI gui;

    if (juce::String (argv[1]) == "--selftest")
        return selfTest();

    LP303Processor processor;
    if (argc >= 3)
        processor.setCurrentProgram (juce::String (argv[2]).getIntValue());
    if (argc >= 4 && juce::String (argv[3]) == "compact")
        processor.sequencerShown = false;

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (auto* constrainer = editor->getConstrainer())
        editor->setSize (constrainer->getMaximumWidth() / 2, constrainer->getMaximumHeight() / 2);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    file.deleteFile();
    juce::FileOutputStream stream (file);
    return juce::PNGImageFormat().writeImageToStream (image, stream) ? 0 : 1;
}
