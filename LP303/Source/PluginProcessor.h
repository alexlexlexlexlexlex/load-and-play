#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "AcidEngine.h"
#include "Sequencer.h"

class LP303Processor final : public juce::AudioProcessor,
                             private juce::Timer
{
public:
    LP303Processor();
    ~LP303Processor() override;

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
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Séquenceur, pour l'interface.
    lp303::PatternBank patterns;
    void patternsEdited();                       // à appeler après une modification d'un motif
    int getPlayingStep() const noexcept          { return sequencer.getPlayingStep(); }
    int getActivePattern() const noexcept        { return sequencer.getActivePattern(); }
    bool isSequencerRunning() const noexcept     { return seqRunning.load(); }
    bool hasHostTransport() const noexcept       { return hostTransport.load(); }

    // Reporte sur les réglages ce que le thread audio a changé (appelé par le timer).
    void flushPendingChanges();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    lp303::Params readParams() const;
    void handleMidi (const juce::MidiMessage&);
    void timerCallback() override { flushPendingChanges(); }

    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool flag (const char* id) const   { return param (id) > 0.5f; }
    int choice (const char* id) const  { return juce::roundToInt (param (id)); }

    lp303::AcidEngine engine;
    lp303::Sequencer sequencer;
    juce::MidiBuffer forEngine;

    double rate = 44100.0;
    double internalPpq = 0.0;
    bool wasSeqOn = false, wasRunning = false;
    int transposeNow = 0, lastTransposeParam = 0;
    std::atomic<int> pendingTranspose { 1000 };   // 1000 = rien à reporter sur le paramètre
    std::atomic<bool> seqRunning { false }, hostTransport { false };
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LP303Processor)
};
