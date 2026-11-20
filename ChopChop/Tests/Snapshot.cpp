// Outil de développement : enregistre une image PNG de l'interface, après avoir
// fait passer quelques secondes de voix et de drums pour que l'écran montre
// quelque chose. Utilisation : Snapshot <sortie.png> [preset]

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

    ChopChopProcessor processor;
    if (argc >= 3)
        processor.setCurrentProgram (juce::String (argv[2]).getIntValue());

    const double rate = 44100.0;
    const int block = 512;
    processor.setRateAndBufferSizeDetails (rate, block);
    processor.prepareToPlay (rate, block);

    // Voix : des syllabes (quelques harmoniques, hauteur qui bouge) ; drums : kick
    // sur chaque temps, caisse claire sur 2 et 4, à 120 BPM.
    std::mt19937 gen (5);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    juce::AudioBuffer<float> buffer (4, block);
    juce::MidiBuffer midi;
    long long t = 0;

    for (int b = 0; b < (int) (4.0 * rate) / block; ++b)
    {
        for (int i = 0; i < block; ++i, ++t)
        {
            const double s = (double) t / rate;
            const double syllable = std::fmod (s, 0.19);
            const float envelope = syllable < 0.14 ? (float) std::sin (juce::MathConstants<double>::pi * syllable / 0.14) : 0.0f;
            const double f0 = 210.0 + 40.0 * std::sin (s * 3.0);
            float voice = 0.0f;
            for (int h = 1; h <= 6; ++h)
                voice += 0.18f / (float) h * (float) std::sin (juce::MathConstants<double>::twoPi * f0 * h * s);
            voice *= envelope;

            const double beat = std::fmod (s, 0.5), bar = std::fmod (s, 2.0);
            float drums = 0.9f * (float) std::exp (-beat * 25.0) * (float) std::sin (juce::MathConstants<double>::twoPi * 55.0 * beat);
            const double snare = std::fmod (bar + 1.5, 1.0);
            drums += 0.5f * (float) std::exp (-snare * 30.0) * noise (gen);

            buffer.setSample (0, i, voice);
            buffer.setSample (1, i, voice);
            buffer.setSample (2, i, drums);
            buffer.setSample (3, i, drums);
        }
        processor.processBlock (buffer, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (ChopChopEditor::designWidth, ChopChopEditor::designHeight);
    for (int i = 0; i < 20; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::cout << "Image : " << file.getFullPathName() << std::endl;
    return 0;
}
