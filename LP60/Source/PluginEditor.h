#pragma once

#include "PluginProcessor.h"

//==============================================================================
// Habillage façon Juno-60 : curseurs verticaux à capuchon coloré, boutons
// carrés à LED rouge, joues en bois.
class PanelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

//==============================================================================
// Sélecteur à LED pour un paramètre à choix (RANGE 16'/8'/4', MODE...) :
// une rangée de boutons dont un seul est allumé.
class LedChoice final : public juce::Component
{
public:
    LedChoice (juce::RangedAudioParameter& parameter, juce::StringArray labels);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::StringArray labels;
    int index = 0;
    juce::ParameterAttachment attachment;
};

//==============================================================================
class LP60Editor final : public juce::AudioProcessorEditor,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    explicit LP60Editor (LP60Processor&);
    ~LP60Editor() override;

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

    // Glisser un fichier MIDI sur la fenêtre ouvre le lecteur.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override              { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    struct Panel final : public juce::Component
    {
        explicit Panel (LP60Editor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintPanel (g); }
        LP60Editor& owner;
    };

    struct Section
    {
        juce::String title;
        int firstColumn, numColumns;
        juce::Colour colour;        // bandeau de titre et filet sous la section
    };

    struct Column
    {
        juce::String title;
        std::vector<juce::Component*> components;   // empilés de haut en bas
        bool isSlider = false;
    };

    void beginSection (const juce::String& title, juce::Colour colour, int firstKeyColour = 0);
    void addSlider (const char* paramId, const juce::String& title);
    void addButtons (std::initializer_list<std::pair<const char*, const char*>> buttons, const juce::String& title = {});
    void addChoice (const char* paramId, const juce::String& title, const juce::StringArray& labels);
    juce::Slider& addKnob (const char* paramId);

    juce::Rectangle<int> columnArea (int column) const;
    void paintPanel (juce::Graphics&);
    void paintHeader (juce::Graphics&, juce::Rectangle<int> area);
    void timerCallback() override;
    void refreshPresetBox();
    void sendWheel (bool pitch, double value);

    LP60Processor& processor;
    PanelLookAndFeel lookAndFeel;
    Panel panel { *this };

    std::vector<Section> sections;
    std::vector<Column> columns;
    int nextKeyColour = 0;

    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::ToggleButton>> buttons;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<LedChoice>> choices;

    std::vector<std::pair<juce::Slider*, juce::String>> knobs;   // réglages du bas, avec leur titre

    juce::ComboBox presetBox;
    juce::TextButton previousPreset { "<" }, nextPreset { ">" };
    juce::Slider pitchWheel { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider modWheel { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::MidiKeyboardComponent keyboard;
    MidiPlayerPanel playerPanel;
    int shownProgram = -1;
    bool dragOver = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LP60Editor)
};
