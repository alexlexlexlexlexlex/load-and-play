#pragma once

#include "PluginProcessor.h"

//==============================================================================
// Habillage de LP-303 : panneau d'alu brossé pour le son (gros boutons noirs à
// index blanc), partie basse noire pour le séquenceur, joues en bois comme sur
// LP-60 et Model Play.
class PanelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

//==============================================================================
// Rangée de boutons à LED pour un paramètre à choix (forme d'onde, motif) :
// un seul allumé à la fois.
class LedChoice final : public juce::Component
{
public:
    LedChoice (juce::RangedAudioParameter& parameter, juce::StringArray labels, int columns, bool onSilver);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    // Motif en cours de lecture, signalé par une petite LED rouge (-1 : aucun).
    void setPlaying (int index);

private:
    juce::Rectangle<float> cellBounds (int index) const;

    juce::StringArray labels;
    int columns;
    bool silver;
    int index = 0, playing = -1;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// Grille du séquenceur : 16 pas en colonnes, les 13 notes (do à do) en lignes
// comme un piano-roll, puis les rangées OCT+, OCT-, ACCENT et SLIDE.
class PatternGrid final : public juce::Component
{
public:
    explicit PatternGrid (LP303Processor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setPattern (int p)        { if (p != pattern) { pattern = p; repaint(); } }
    void setPlayingStep (int s)    { if (s != playingStep) { playingStep = s; repaint(); } }

    static constexpr int numToggleRows = 4;

private:
    enum class Row { none, header, note, up, down, accent, slide };

    struct Hit
    {
        Row row = Row::none;
        int step = -1, note = -1;
    };

    juce::Rectangle<float> gridArea() const;
    float stepWidth() const;
    juce::Rectangle<float> noteCell (int step, int note) const;
    juce::Rectangle<float> toggleCell (int step, int toggleRow) const;
    Hit findHit (juce::Point<float>) const;
    void apply (const Hit&, bool firstClick);

    LP303Processor& processor;
    int pattern = 0, playingStep = -1;

    Row dragRow = Row::none;
    bool dragValue = false;
    int lastDragStep = -1;
    bool edited = false;
};

//==============================================================================
class LP303Editor final : public juce::AudioProcessorEditor,
                          public juce::FileDragAndDropTarget,
                          private juce::Timer
{
public:
    explicit LP303Editor (LP303Processor&);
    ~LP303Editor() override;

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

    // Glisser un fichier MIDI sur la fenêtre ouvre le lecteur.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override              { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

    // Séquenceur affiché, ou replié : la fenêtre ne garde que le son et le clavier.
    void showSequencer (bool shown);
    bool isSequencerShown() const noexcept { return seqShown; }

private:
    // Le panneau est dessiné à taille fixe puis mis à l'échelle de la fenêtre.
    struct Panel final : public juce::Component
    {
        explicit Panel (LP303Editor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintPanel (g); }
        LP303Editor& owner;
    };

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::String title;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    Knob& addKnob (const char* paramId, const juce::String& title, bool onSilver);
    void paintPanel (juce::Graphics&);
    void paintHeader (juce::Graphics&);
    void timerCallback() override;
    void refreshPresetBox();
    int editedPattern() const;
    void editPattern (const std::function<void (lp303::PatternBank&, int)>& change);
    int panelHeight() const;
    int keyboardY() const;
    void layoutBottom();

    LP303Processor& processorRef;
    PanelLookAndFeel lookAndFeel;
    Panel panel { *this };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<LedChoice> waveChoice, patternChoice;
    PatternGrid grid;

    juce::ToggleButton runButton { "RUN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> runAttachment;

    juce::TextButton lengthDown { "-" }, lengthUp { "+" };
    juce::TextButton clearButton { "CLEAR" }, randomButton { "RANDOM" }, copyButton { "COPY" }, pasteButton { "PASTE" };
    juce::ComboBox presetBox;
    juce::TextButton previousPreset { "<" }, nextPreset { ">" };
    juce::MidiKeyboardComponent keyboard;
    MidiPlayerPanel playerPanel;
    juce::TextButton seqButton;
    std::vector<juce::Component*> seqComponents;     // tout ce qui disparaît quand le séquenceur est replié
    bool seqShown = true, dragOver = false;

    juce::String clipboard;
    juce::uint64 gridHash = 0;
    int shownProgram = -1, shownLength = -1;
    bool shownHost = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LP303Editor)
};
