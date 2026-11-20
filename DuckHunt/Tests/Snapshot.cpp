// Outil de développement : enregistre une image PNG de l'interface, après
// avoir fait passer quelques secondes de son (piste + référence) pour que
// l'écran montre quelque chose. Utilisation : Snapshot <sortie.png> [preset]

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

#include <random>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::cerr << "usage: Snapshot <sortie.png> [preset]" << std::endl;
        return 1;
    }

    DuckHuntProcessor processor;
    if (argc >= 3)
        processor.setCurrentProgram (juce::String (argv[2]).getIntValue());

    const double rate = 44100.0;
    const int block = 512;
    processor.setRateAndBufferSizeDetails (rate, block);
    processor.prepareToPlay (rate, block);

    // Piste : accord et bruit ; référence : un kick par temps à 120 BPM, ou une « voix »
    // (quelques harmoniques autour de 200 Hz) en mode spectral.
    const bool spectral = processor.state.getRawParameterValue ("mode")->load() > 1.5f;
    std::mt19937 gen (5);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    juce::AudioBuffer<float> buffer (4, block);
    juce::MidiBuffer midi;
    long long t = 0;

    for (int b = 0; b < (int) (3.0 * rate) / block; ++b)
    {
        for (int i = 0; i < block; ++i, ++t)
        {
            const double s = (double) t / rate;
            float main = 0.08f * noise (gen);
            for (double f : { 110.0, 220.0, 277.2, 329.6, 440.0, 1760.0 })
                main += 0.06f * (float) std::sin (juce::MathConstants<double>::twoPi * f * s);

            float side = 0.0f;
            if (spectral)
            {
                const float swell = 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 0.7 * s);
                for (int h = 1; h <= 8; ++h)
                    side += swell * 0.25f / (float) h * (float) std::sin (juce::MathConstants<double>::twoPi * 196.0 * h * s);
            }
            else
            {
                const double beat = std::fmod (s, 0.5);
                side = 0.9f * (float) std::exp (-beat * 18.0) * (float) std::sin (juce::MathConstants<double>::twoPi * 55.0 * beat * (1.0 + 2.0 * std::exp (-beat * 40.0)));
            }

            buffer.setSample (0, i, main);
            buffer.setSample (1, i, main);
            buffer.setSample (2, i, side);
            buffer.setSample (3, i, side);
        }
        processor.processBlock (buffer, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (DuckHuntEditor::designWidth, DuckHuntEditor::designHeight);
    for (int i = 0; i < 20; ++i)   // laisse l'écran lire l'historique
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::cout << "Image : " << file.getFullPathName() << std::endl;
    return 0;
}
