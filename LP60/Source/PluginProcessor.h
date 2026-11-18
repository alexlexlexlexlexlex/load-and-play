#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "Arpeggiator.h"
#include "Chorus.h"
#include "LP60Engine.h"
#include "MidiFilePlayer.h"

class LP60Processor final : public juce::AudioProcessor
{
public:
    LP60Processor();

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
    double getTailLengthSeconds() const override { return 20.0; }

    // Sons d'usine, accessibles depuis l'hôte et depuis l'interface.
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    juce::MidiMessageCollector uiMidi;      // levier et molette de l'interface
    MidiFilePlayer midiPlayer;              // fichier MIDI glissé sur la fenêtre

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    lp60::Params readParams() const;
    lp60::ArpSettings readArp() const;
    void applyHold (juce::MidiBuffer& midi);
    void handleMidi (const juce::MidiMessage&);

    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool flag (const char* id) const   { return param (id) > 0.5f; }
    int choice (const char* id) const  { return juce::roundToInt (param (id)); }

    lp60::Engine engine;
    lp60::Chorus chorus;
    lp60::Arpeggiator arpeggiator;
    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer fromUi, holdOutput;

    // HOLD et pédale de sustain : les notes relâchées restent tenues.
    std::array<bool, 128> keyDown {}, latched {};
    bool sustainPedal = false, wasHolding = false;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LP60Processor)
};
