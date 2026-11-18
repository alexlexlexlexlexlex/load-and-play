#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "Arpeggiator.h"
#include "Effects.h"
#include "MidiFilePlayer.h"
#include "SynthEngine.h"

class ModelDProcessor final : public juce::AudioProcessor,
                              private juce::Timer
{
public:
    ModelDProcessor();
    ~ModelDProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // MIDI learn : on désigne un réglage, puis le premier contrôleur (CC) qui
    // bouge lui est associé. Les associations sont sauvegardées avec le projet.
    void setLearnTarget (const juce::String& paramId);
    void cancelLearn()                       { learnTarget = -1; }
    juce::String getLearnTarget() const;
    void clearMidiMap();
    int getCcFor (const juce::String& paramId) const;
    juce::String getLastLearned() const;    // « CC 21 → Cutoff Frequency », vide sinon

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    juce::MidiMessageCollector uiMidi;      // molettes de l'interface
    MidiFilePlayer midiPlayer;              // fichier MIDI glissé sur la fenêtre

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    modeld::Params readParams() const;
    modeld::EffectsParams readEffects() const;
    modeld::ArpSettings readArp() const;
    void handleMidi (const juce::MidiMessage&);
    void handleControllers (const juce::MidiBuffer&);
    void timerCallback() override;

    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool flag (const char* id) const   { return param (id) > 0.5f; }
    int choice (const char* id) const  { return juce::roundToInt (param (id)); }
    int indexOf (const juce::String& paramId) const;

    modeld::SynthEngine engine;
    modeld::Effects effects;
    modeld::Arpeggiator arpeggiator;
    juce::AudioBuffer<float> stereo;
    juce::MidiBuffer fromUi;
    std::array<std::atomic<float>*, 16> arpSteps {};

    juce::Array<juce::RangedAudioParameter*> parameters;
    std::array<std::atomic<int>, 128> ccMap;        // numéro de CC -> index de paramètre, -1 si libre
    std::array<std::atomic<int>, 128> pendingCc;    // dernière valeur reçue, -1 si rien de neuf
    std::atomic<int> learnTarget { -1 };
    std::atomic<int> lastLearnedCc { -1 }, lastLearnedParam { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModelDProcessor)
};
