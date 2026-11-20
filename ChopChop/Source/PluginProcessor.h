#pragma once

#include "ChopEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

class ChopChopProcessor final : public juce::AudioProcessor
{
public:
    ChopChopProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }   // échos et reverb

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    bool drumsConnected() const { return sidechainActive.load(); }
    static juce::String stepId (int index) { return "step" + juce::String (index + 1); }

    juce::AudioProcessorValueTreeState state;
    chopchop::ChopEngine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    chopchop::Params readParams() const;

    double sampleRate = 44100.0;
    int currentProgram = 0;
    std::atomic<bool> sidechainActive { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChopChopProcessor)
};
