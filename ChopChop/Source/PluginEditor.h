#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace Colours
{
const juce::Colour panel { 0xff1b1b1b }, screen { 0xff0d0d0d }, text { 0xfff0ede4 }, dim { 0xff8c877d },
                   accent { 0xffe8862c }, wood { 0xff5a3418 }, line { 0xff343434 }, main { 0xff5e5e5e };

// Une couleur par type de pas (THRU, REPEAT, ROLL, REVERSE, +12, -12, MUTE).
juce::Colour forStep (int type);
}

class PanelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};

class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& name);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String name;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class Switch final : public juce::Component
{
public:
    Switch (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& name);
    void resized() override { button.setBounds (getLocalBounds()); }

private:
    juce::ToggleButton button;
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

class Segmented final : public juce::Component
{
public:
    Segmented (juce::RangedAudioParameter&, const juce::StringArray& labels);
    void resized() override;
    int selected() const { return current; }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment;
    int current = 0;
};

// Les 16 pas : un clic passe au type suivant (Maj : précédent), glisser peint le même
// type sur les pas voisins, clic droit : menu, double-clic : THRU.
class StepSequencer final : public juce::Component,
                            private juce::Timer
{
public:
    explicit StepSequencer (ChopChopProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void clear();
    void randomise();

private:
    void timerCallback() override;
    int cellAt (juce::Point<float>) const;
    juce::Rectangle<float> cellBounds (int index) const;
    void setStep (int index, int type);

    ChopChopProcessor& processor;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> attachments;
    std::array<int, chopchop::numSteps> types {};
    int paintType = 0, lastPainted = -1, playing = -1;
    juce::Random random;
};

// Écran : voix d'entrée, sortie, drums et ce qui joue, dans le temps.
class Display final : public juce::Component,
                      private juce::Timer
{
public:
    explicit Display (ChopChopProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    ChopChopProcessor& processor;
    static constexpr int numPoints = 400;   // 4 s
    std::array<chopchop::HistoryPoint, numPoints> points {};
    int head = 0;
    float recentDrums = 0.0f;
};

class ChopChopEditor final : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit ChopChopEditor (ChopChopProcessor&);
    ~ChopChopEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 1040, designHeight = 570;

private:
    class Content;
    void timerCallback() override;

    PanelLookAndFeel lookAndFeel;
    std::unique_ptr<Content> content;
};
