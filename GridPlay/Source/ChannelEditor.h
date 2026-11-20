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
    const int trackId;
    juce::String name;
    juce::Colour colour;
    ChannelEq currentEq;
    float meter = 0.0f;

    std::unique_ptr<EqGraph> graph;
    juce::OwnedArray<InsertRow> rows;
    juce::OwnedArray<BandControls> bands;
    juce::TextButton eqBypass { "EQ" }, eqReset;
    juce::Slider fader, pan;
    juce::Label faderValue;
    juce::Rectangle<int> insertsArea, eqArea, channelArea, meterArea;
    juce::TooltipWindow tooltips { this, 450 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelEditor)
};
} // namespace gridplay
