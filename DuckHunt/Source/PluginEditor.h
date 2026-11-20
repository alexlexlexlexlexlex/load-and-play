#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace Colours
{
const juce::Colour panel { 0xff1b1b1b }, screen { 0xff0d0d0d }, text { 0xfff0ede4 }, dim { 0xff8c877d },
                   accent { 0xffe8862c }, wood { 0xff5a3418 }, line { 0xff343434 }, main { 0xff5e5e5e },
                   low { 0xffff6a5c }, mid { 0xfff2d34f }, high { 0xff6fb7ff };
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
    juce::Font getLabelFont (juce::Label&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};

// Potard avec son nom au-dessus et sa valeur en dessous.
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

// Interrupteur orange avec son nom en dessous.
class Switch final : public juce::Component
{
public:
    Switch (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& name);
    void resized() override { button.setBounds (getLocalBounds()); }

private:
    juce::ToggleButton button;
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

// Rangée de boutons pour un choix (déclenchement, mode).
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

// Écran : niveaux et baisse dans le temps, ou spectre en mode spectral.
class Display final : public juce::Component,
                      private juce::Timer
{
public:
    explicit Display (DuckHuntProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void paintTime (juce::Graphics&, juce::Rectangle<float>, const duckhunt::Params&);
    void paintSpectrum (juce::Graphics&, juce::Rectangle<float>, const duckhunt::Params&);

    DuckHuntProcessor& processor;
    static constexpr int numPoints = 360;   // 3,6 s
    std::array<duckhunt::HistoryPoint, numPoints> points {};
    int head = 0;
    duckhunt::SpectrumView spectrum, smoothed;
    float recentSide = 0.0f;
};

class DuckHuntEditor final : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit DuckHuntEditor (DuckHuntProcessor&);
    ~DuckHuntEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 920, designHeight = 616;

private:
    class Content;
    void timerCallback() override;

    PanelLookAndFeel lookAndFeel;
    std::unique_ptr<Content> content;
};
