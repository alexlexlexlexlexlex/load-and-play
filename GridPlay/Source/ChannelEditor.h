// Les réglages d'une voie (bouton « e », comme dans Cubase) : les 8 inserts d'effets,
// l'égaliseur avec sa courbe (points à tirer à la souris), le fader, le panoramique et
// le vu-mètre. Une fenêtre par piste, et une pour le master.
#pragma once

#include "AudioEngine.h"
#include "Theme.h"

namespace gridplay
{
class ChannelEditor : public juce::Component, public juce::DragAndDropContainer, private juce::Timer
{
public:
    // trackId : Track::id de la piste, ou masterId pour le master.
    static constexpr int masterId = -1;
    ChannelEditor (AudioEngine& engine, int trackId);
    ~ChannelEditor() override;

    int getTrackId() const { return trackId; }
    juce::String getTitle() const;

    std::function<void (int target, int slot)> onOpenInsert;   // ouvrir la fenêtre d'un effet
    std::function<void()> onScanPlugins;
    std::function<void()> onGone;                              // la piste a été supprimée
    std::function<void()> onTrackChanged;                      // flèches : autre piste

    // Annuler / rétablir les réglages de cette voie (égaliseur, volume, panoramique, inserts).
    void undo();
    void redo();
    void remember();                          // avant une modification (les gestes continus comptent pour un)
    void rememberMove (int from, int to);     // un effet change de case
    void navigate (int direction);            // piste précédente / suivante
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class EqGraph;
    class InsertRow;
    class BandControls;
    class TypeButton;

    void timerCallback() override;
    int target() const;     // index de la piste, ou AudioEngine::masterTarget
    void editEq (const std::function<void (ChannelEq&)>& change);
    ChannelEq readEq() const;
    void refresh();
    void showInsertMenu (int slot, bool filled);

    AudioEngine& engine;
    int trackId;
    juce::String name;
    juce::Colour colour;
    ChannelEq currentEq;
    float meter = 0.0f;

    std::unique_ptr<EqGraph> graph;
    juce::OwnedArray<InsertRow> rows;
    juce::OwnedArray<BandControls> bands;
    juce::TextButton eqBypass { "EQ" }, eqReset;
    struct Snapshot
    {
        ChannelEq eq;
        float volume = 0.85f, pan = 0.0f;
        std::vector<bool> bypass;
        int moveFrom = -1, moveTo = -1;   // un déplacement d'effet, au lieu d'un état
    };
    Snapshot capture() const;
    void restore (const Snapshot&);
    std::vector<Snapshot> undoStack, redoStack;
    juce::uint32 lastEdit = 0;

    juce::Slider fader, pan;
    juce::Label faderValue, peakValue;
    juce::TextButton undoButton, redoButton, prevButton, nextButton, muteButton { "M" }, soloButton { "S" };
    float peakHold = 0.0f;
    juce::Rectangle<int> insertsArea, eqArea, channelArea, meterArea, scaleArea;
    juce::TooltipWindow tooltips { this, 450 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelEditor)
};
} // namespace gridplay
