#pragma once

#include "PluginProcessor.h"

//==============================================================================
// Habillage façon façade de Model D : boutons noirs à jupe crantée, chapeau
// alu, graduations 0-10 blanches, interrupteurs à bascule, molettes.
class PanelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

//==============================================================================
// Pas du motif d'arpège : une LED rouge allumée quand le pas joue.
class StepButton final : public juce::ToggleButton
{
public:
    using juce::ToggleButton::ToggleButton;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

//==============================================================================
class ModelDEditor final : public juce::AudioProcessorEditor,
                          public juce::FileDragAndDropTarget,
                          private juce::Timer
{
public:
    explicit ModelDEditor (ModelDProcessor&);
    ~ModelDEditor() override;

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

    // Glisser un fichier MIDI sur la fenêtre ouvre le lecteur.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override              { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    // Le panneau est dessiné à taille fixe puis mis à l'échelle de la fenêtre.
    struct Panel final : public juce::Component
    {
        explicit Panel (ModelDEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override             { owner.paintPanel (g); }
        void paintOverChildren (juce::Graphics& g) override { owner.paintOverlay (g); }
        ModelDEditor& owner;
    };

    struct Control
    {
        juce::String paramId;
        juce::Component* component = nullptr;
    };

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Switch
    {
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        int band, firstColumn, numColumns;
    };

    enum Kind { full = -1, top = 0, bottom = 1, knobLabel = -2, knobBody = -3 };

    struct Placement
    {
        juce::Component* component;
        int band, column, row, kind;
    };

    void addKnob (const char* paramId, const juce::String& text, int band, int column, int row);
    void addSwitch (const char* paramId, const juce::String& text, int band, int column, int row, int kind = full);
    juce::Rectangle<int> cell (int band, int column, int row) const;
    int bandTop (int band) const;
    juce::Rectangle<int> stepArea() const;

    void paintPanel (juce::Graphics&);
    void paintOverlay (juce::Graphics&);
    void mouseDown (const juce::MouseEvent&) override;
    void timerCallback() override;
    void sendWheel (bool pitch, double value);

    ModelDProcessor& processor;
    PanelLookAndFeel lookAndFeel;
    Panel panel { *this };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::vector<Placement> placements;
    std::vector<Control> controls;
    std::vector<Section> sections;

    juce::TextButton learnButton { "LEARN" }, resetButton { "RESET" };
    juce::Slider pitchWheel { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider modWheel { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::MidiKeyboardComponent keyboard;
    MidiPlayerPanel playerPanel;
    bool dragOver = false;

    juce::String learnTarget, status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModelDEditor)
};
