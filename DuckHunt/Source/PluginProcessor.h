#pragma once

#include "DuckEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

class DuckHuntProcessor final : public juce::AudioProcessor,
                                private juce::Timer
{
public:
    DuckHuntProcessor();
    ~DuckHuntProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // La piste de référence est-elle branchée (entrée sidechain active) ?
    bool sidechainConnected() const { return sidechainActive.load(); }

    juce::AudioProcessorValueTreeState state;
    duckhunt::DuckEngine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    duckhunt::Params readParams() const;
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&, bool bypassed);
    void timerCallback() override;

    double sampleRate = 44100.0;
    int currentProgram = 0;
    std::atomic<int> wantedLatency { 0 };
    std::atomic<bool> sidechainActive { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DuckHuntProcessor)
};
