// L'éditeur de notes, sur le modèle du Key Editor de Cubase :
// barre d'outils, ligne d'infos de la note sélectionnée, clavier (ou drum map),
// règle avec le curseur de lecture et la fin de boucle, grille des notes, vélocités.
// Clic droit maintenu : boîte à outils, comme dans Cubase. Raccourcis : voir README.
#pragma once

#include "AudioEngine.h"
#include "NoteOps.h"
#include "Theme.h"
#include "TransportControls.h"

namespace gridplay
{
class NoteEditor : public juce::Component,
                   private juce::Timer,
                   private juce::ScrollBar::Listener
{
public:
    NoteEditor (AudioEngine& engine, ClipPtr clip);
    ~NoteEditor() override;

    void setClip (ClipPtr clip);
    ClipPtr getClip() const { return clip; }

    std::function<void()> onClipGone;              // le clip a été supprimé de la grille
    std::function<void()> onCloseRequested;        // Cmd W / Échap sans sélection

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    enum class Tool { select, split, glue, erase, zoom, mute, draw };
    void setTool (Tool t);
    static void drawToolIcon (juce::Graphics&, Tool, juce::Rectangle<float>, juce::Colour);
    static juce::String toolName (Tool);

    static constexpr int toolbarH = 48, infoH = 32, rulerH = 26, pianoW = 64, drumW = 150, velocityH = 92, gap = 6, barT = 10;

private:
    class ToolButton;
    class InfoField;
    class Toolbox;
    class QuantizePanel;

    enum class Drag { none, move, resizeStart, resizeEnd, velocity, rubber, draw, erase, loopEnd, cursor, velocityLane, keyboard, toolbox };

    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;

    // Modifications : passent par commit(), qui les envoie au séquenceur sous verrou.
    void pushUndo();
    void commit (bool undoable, const std::function<void()>& change);
    void sendToClip();
    void undo();
    void redo();

    // Lignes (une par hauteur au piano, une par son en mode batterie)
    void rebuildRows();
    int rowOf (int pitch) const;
    int pitchAtRow (int row) const;
    int numRows() const { return (int) rows.size(); }
    int keysWidth() const { return drumMode ? drumW : pianoW; }

    // Coordonnées
    double beatAtX (float x) const;
    float xForBeat (double beat) const;
    int rowAtY (float y) const;
    int pitchAtY (float y) const;
    float yForPitch (int pitch) const;
    juce::Rectangle<float> noteBounds (const Note& n) const;
    int noteAt (juce::Point<float> p) const;
    double grid() const;
    double insertLength() const;
    double contentBeats() const;
    void updateScrollBars();
    void zoomHorizontal (double factor, float anchorX);
    void zoomVertical (double factor, float anchorY);
    void scrollToNotes();
    void followPlayhead();

    // Actions
    void quantize();
    void selectAll();
    void deleteSelection();
    void copySelection (bool cut);
    void paste();
    void duplicateSelection();
    void moveRows (int rowDelta);
    void nudgeSelection (double beats);
    void resizeSelection (double beats);
    void selectNeighbour (int direction, bool extend);
    void toggleMute (int index);
    void setLoopLength (double beats);
    void openToolbox (juce::Point<int> at);
    void closeToolbox();
    void runCommand (int id);
    void showQuantizePanel();
    void updateInfo();
    void updateToolbar();
    int firstSelected() const;

    // Lecture (comme le transport de Cubase : depuis le curseur)
    void playFromCursor();
    void stopPlayback();
    void togglePlayback();
    void moveCursor (double beat);

    // Écoute
    void preview (int pitch, int velocity);
    void stopPreview();
    void previewBriefly (int pitch, int velocity);

    // Dessin
    void paintGrid (juce::Graphics&);
    void paintNotes (juce::Graphics&);
    void paintKeys (juce::Graphics&);
    void paintRuler (juce::Graphics&);
    void paintVelocity (juce::Graphics&);

    // Où est le clip dans la grille (pour la piste, l'écoute et la tête de lecture).
    struct Location { int track = -1, scene = -1; bool playing = false, queued = false; double position = 0.0; bool drumMap = false; };
    Location locate() const;

    AudioEngine& engine;
    ClipPtr clip;
    std::vector<Note> notes;
    double length = 4.0;
    notes::Selection selection;
    Location location;
    bool drumMode = false;
    std::vector<int> rows;                 // hauteur de chaque ligne, de haut en bas
    std::array<int, 128> rowForPitch {};   // -1 si la hauteur n'a pas de ligne

    Tool tool = Tool::select;
    bool snap = true;
    int gridIndex = 4;          // 1/16 : calage et quantification (comme « Utiliser la quantification » de Cubase)
    int lengthIndex = 0;        // 0 = longueur de la quantification
    int insertVelocity = 100;
    bool listen = true;
    notes::QuantizeSettings quantizeSettings;

    double pixelsPerBeat = 60.0;
    float rowHeight = 14.0f;
    double scrollX = 0.0, scrollY = 0.0;
    bool viewInitialised = false;
    juce::Rectangle<int> toolbarArea, infoArea, rulerArea, gridArea, keysArea, velocityArea;

    double cursorBeat = 0.0;    // le curseur de lecture (ligne blanche), déplacé dans la règle
    bool wasPlaying = false;
    double cursorBeatWhenPlaying = 0.0;
    bool rulerZooming = false;          // glisser vers le haut / bas dans la règle : zoom (comme Cubase)
    double zoomStartPixels = 60.0, zoomAnchorBeat = 0.0;

    Drag drag = Drag::none;
    int dragIndex = -1;
    juce::Point<float> dragStart;
    std::vector<Note> original;
    notes::Selection baseSelection;
    bool copyOnDrag = false, dragChanged = false;
    juce::Rectangle<float> rubber;
    int previewPitch = -1;
    juce::uint32 toolboxOpenedAt = 0;

    std::vector<std::pair<std::vector<Note>, double>> undoStack, redoStack;

    juce::OwnedArray<ToolButton> tools;
    TransportButton playButton { TransportButton::Icon::play }, stopButton { TransportButton::Icon::stop };
    juce::TextButton snapButton { "Calage" }, quantizeButton { "Q" }, quantizeMore { "..." }, listenButton, drumButton { "Batterie" };
    juce::TextButton loopMinus { "-" }, loopPlus { "+" };
    juce::ComboBox gridBox, lengthBox;
    juce::Label lengthLabel, velocityLabel, loopLabel, title;
    std::unique_ptr<InfoField> velocityField, loopField;
    juce::OwnedArray<InfoField> info;
    juce::OwnedArray<juce::Label> infoLabels;
    juce::ScrollBar hBar { false }, vBar { true };
    std::unique_ptr<Toolbox> toolbox;
    juce::TooltipWindow tooltipWindow { this, 450 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteEditor)
};
} // namespace gridplay
