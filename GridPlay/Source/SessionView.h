// La vue « session » : la grille de clips (pistes en colonnes, scènes en lignes),
// la colonne des scènes à droite, les boutons d'arrêt, et la console des pistes
// en bas (entrée MIDI, sortie, armement, mute, solo, volume).
#pragma once

#include "AudioEngine.h"
#include "Theme.h"

namespace gridplay
{
// Liste déroulante qui se remplit au moment de s'ouvrir (appareils branchés, instruments).
class LazyCombo : public juce::ComboBox
{
public:
    std::function<void (LazyCombo&)> onFill;
    void showPopup() override
    {
        if (onFill)
            onFill (*this);
        juce::ComboBox::showPopup();
    }
};

// Ce que l'écran affiche, relevé 30 fois par seconde sous le verrou.
struct SessionSnapshot
{
    struct TrackInfo
    {
        juce::String name;
        juce::Colour colour;
        bool arm = false, mute = false, solo = false, active = false;
        double progress = 0.0;
        juce::String inputDevice;
        bool inputEnabled = true;
        int inputChannel = 0;
        OutputType outputType = OutputType::none;
        juce::String outputPort, pluginName;
        int outputChannel = 1;
        float volume = 0.85f, pan = 0.0f, meter = 0.0f;
        int audioInput = -1;
        bool audioInputStereo = true;
        int inserts = 0;
        bool eqActive = false;
        double delayMs = 0.0;
        int transpose = 0, velocityShift = 0, program = -1;
        bool drumMap = false;
    };
    struct SceneInfo { juce::String name; bool active = false; };
    struct CellInfo { SlotState state = SlotState::empty; juce::String name; juce::Colour colour; int notes = 0; };

    std::vector<TrackInfo> tracks;
    std::vector<SceneInfo> scenes;
    std::vector<CellInfo> cells;   // piste * nbScènes + scène
    bool playing = false;
    double beat = 0.0, tempo = 120.0;
    float master = 0.85f, masterMeter = 0.0f;

    const CellInfo& cell (int t, int s) const { return cells[(size_t) (t * (int) scenes.size() + s)]; }
};

class SessionView : public juce::Component,
                    public juce::FileDragAndDropTarget,
                    private juce::Timer,
                    private juce::ScrollBar::Listener
{
public:
    explicit SessionView (AudioEngine& engine);
    ~SessionView() override;

    // Commandes (menus et raccourcis).
    void launchSelected();
    void deleteSelected();
    void duplicateSelected();
    void copySelected();
    void pasteSelected();
    void renameSelected();
    void editSelected();
    void insertTrack();
    void insertScene();
    void selectCell (int track, int scene);

    // Annuler / rétablir dans la grille : clips, noms et couleurs des pistes et des scènes.
    void undo();
    void redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

    // Pour les menus.
    int getSelectedTrack() const { return selTrack; }
    int getSelectedScene() const { return selScene; }
    void armSelectedTrack();
    void deleteSelectedTrack();
    void deleteSelectedScene();
    void renameSelectedTrack();
    void launchSelectedScene();
    void importMidiIntoSelected();
    void exportSelectedClip();
    bool hasClipSelected() const;

    std::function<void (int track)> onOpenPlugin;
    std::function<void (int track)> onOpenChannel;     // réglages de voie (-1 : master)
    std::function<void()> onScanPlugins;
    std::function<void (int track, int scene)> onEditClip;   // double-clic sur un clip

    const SessionSnapshot& getSnapshot() const { return view; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    static constexpr int headerH = 26, slotH = 28, trackW = 128, sceneW = 140, stopH = 30, compactMixerH = 214, detailedMixerH = 400, barT = 10;

    // Console du bas : compacte, ou détaillée comme l'inspecteur de Cubase.
    void setDetailed (bool detailed);
    bool isDetailed() const { return detailed; }
    std::function<void()> onLayoutChanged;

private:
    class TrackStrip;
    class MasterStrip;

    struct Hit
    {
        enum class Kind { none, header, addTrack, slot, slotButton, addScene, scene, sceneButton, trackStop, stopAll };
        Kind kind = Kind::none;
        int track = -1, scene = -1;
    };

    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void takeSnapshot();
    std::uint64_t signature() const;
    void updateStrips();
    void updateScrollBars();
    Hit findHit (juce::Point<int> p) const;
    int gridRight() const     { return getWidth() - sceneW - barT; }
    int slotsBottom() const   { return getHeight() - mixerH - barT - stopH; }
    juce::Rectangle<int> slotBounds (int track, int scene) const;
    void showSlotMenu (int track, int scene);
    void showTrackMenu (int track);
    void showSceneMenu (int scene);
    void editInline (juce::Rectangle<int> area, const juce::String& initial, std::function<void (const juce::String&)> done);
    void finishInlineEdit (bool keep);
    juce::Rectangle<int> headerBounds (int track) const;
    juce::Rectangle<int> sceneBounds (int scene) const;
    void importMidiFiles (const juce::StringArray& files, int track, int scene);
    void exportClip (int track, int scene);
    void createEmptyClip (int track, int scene, int bars);
    void ensureVisible (int track, int scene);

    AudioEngine& engine;
    SessionSnapshot view;
    int scrollX = 0, scrollY = 0;
    juce::ScrollBar hBar { false }, vBar { true };
    juce::Component stripHolder;
    juce::OwnedArray<TrackStrip> strips;
    std::unique_ptr<MasterStrip> master;

    int selTrack = 0, selScene = 0;
    bool detailed = false;
    int mixerH = compactMixerH;
    std::uint64_t lastSignature = 0;
    bool dragging = false;
    int dragTrack = -1, dragScene = -1, dropTrack = -1, dropScene = -1;
    bool dragCopy = false;
    juce::Point<int> dragStart;
    ClipPtr clipboard;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::TextEditor> inlineEditor;

    struct GridState
    {
        struct TrackLook { juce::String name; juce::Colour colour; std::vector<ClipPtr> slots; };
        std::vector<TrackLook> tracks;
        std::vector<juce::String> scenes;
    };
    GridState captureGrid() const;
    bool restoreGrid (const GridState& state);
    void rememberGrid();
    std::vector<GridState> undoStack, redoStack;
    std::function<void (const juce::String&)> inlineDone;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionView)
};
} // namespace gridplay
