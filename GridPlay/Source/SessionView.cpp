#include "SessionView.h"

#include <cstring>

namespace gridplay
{
namespace
{
    const char* colourNames[] = { "Rose", "Orange", "Ocre", "Jaune pâle", "Citron", "Vert", "Menthe", "Turquoise",
                                  "Ciel", "Bleu", "Lavande", "Violet", "Magenta", "Blanc", "Rouge", "Mandarine" };

    juce::String channelName (int channel) { return "Canal " + juce::String (channel); }

    void drawTriangle (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        juce::Path p;
        const auto cx = r.getCentreX(), cy = r.getCentreY();
        p.addTriangle (cx - 3.5f, cy - 5.0f, cx - 3.5f, cy + 5.0f, cx + 5.0f, cy);
        g.setColour (c);
        g.fillPath (p);
    }

    juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }
}

//==============================================================================
// Une tranche de console : entrée, sortie, armement, mute, solo, volume.
class SessionView::TrackStrip : public juce::Component
{
public:
    TrackStrip (SessionView& o) : owner (o)
    {
        for (auto* c : { (juce::ComboBox*) &input, (juce::ComboBox*) &inChannel, (juce::ComboBox*) &output, (juce::ComboBox*) &outChannel })
            addAndMakeVisible (c);
        for (auto* b : { &edit, &arm, &mute, &solo })
            addAndMakeVisible (b);
        addAndMakeVisible (volume);

        input.setTooltip (juce::String::fromUTF8 ("Entrée MIDI de la piste : le clavier joue la piste quand elle est armée"));
        inChannel.setTooltip (juce::String::fromUTF8 ("Canal MIDI d'entrée"));
        output.setTooltip (juce::String::fromUTF8 ("Sortie : un instrument virtuel, ou un port MIDI (synthé, autre logiciel)"));
        outChannel.setTooltip ("Canal MIDI de sortie");
        edit.setTooltip (juce::String::fromUTF8 ("Ouvrir la fenêtre de l'instrument"));
        arm.setTooltip ("Armer : le clavier joue la piste, et un clic sur une case vide enregistre");
        mute.setTooltip ("Couper la piste");
        solo.setTooltip ("Solo");

        arm.setClickingTogglesState (false);
        arm.setColour (juce::TextButton::buttonOnColourId, theme::record);
        mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd8c84a));
        solo.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff5aa9e6));

        volume.setSliderStyle (juce::Slider::LinearHorizontal);
        volume.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        volume.setRange (0.0, 1.0);
        volume.setDoubleClickReturnValue (true, 0.85);
        volume.textFromValueFunction = [] (double v)
        {
            const auto db = volumeToDecibels ((float) v);
            return db <= -69.0f ? juce::String ("-inf") : juce::String (db, 1) + " dB";
        };
        volume.valueFromTextFunction = [] (const juce::String& text)
        {
            const float db = text.getFloatValue();
            return (double) (0.85f * std::sqrt (juce::Decibels::decibelsToGain (db)));
        };
        volume.setTooltip ("Volume de la piste (CC 7 pour un port MIDI)");

        input.onFill = [this] (LazyCombo& c)
        {
            c.clear (juce::dontSendNotification);
            inputNames = owner.engine.getInputNames();
            c.addItem ("Tous les claviers", 1);
            c.addItem (juce::String::fromUTF8 ("Aucune entrée"), 2);
            c.addSeparator();
            for (int i = 0; i < inputNames.size(); ++i)
                c.addItem (inputNames[i], 100 + i);
            const auto& t = info();
            c.setSelectedId (! t.inputEnabled ? 2 : t.inputDevice.isEmpty() ? 1 : 100 + inputNames.indexOf (t.inputDevice), juce::dontSendNotification);
        };
        input.onChange = [this]
        {
            const int id = input.getSelectedId();
            if (id <= 0) return;
            const auto name = id >= 100 ? inputNames[id - 100] : juce::String();
            owner.engine.edit ([&] (Session& s)
            {
                auto& t = trackAt (s);
                t.inputEnabled = id != 2;
                t.inputDevice = name;
            });
        };

        inChannel.onFill = [this] (LazyCombo& c)
        {
            c.clear (juce::dontSendNotification);
            c.addItem ("Tous canaux", 1);
            for (int ch = 1; ch <= 16; ++ch)
                c.addItem (channelName (ch), ch + 1);
            c.setSelectedId (info().inputChannel + 1, juce::dontSendNotification);
        };
        inChannel.onChange = [this]
        {
            const int id = inChannel.getSelectedId();
            if (id > 0)
                owner.engine.edit ([&] (Session& s) { trackAt (s).inputChannel = id - 1; });
        };

        output.onFill = [this] (LazyCombo& c)
        {
            c.clear (juce::dontSendNotification);
            instruments.clear();
            for (const auto& d : owner.engine.knownPlugins.getTypes())
                if (d.isInstrument)
                    instruments.add (d);
            std::sort (instruments.begin(), instruments.end(), [] (const auto& a, const auto& b)
                       { return a.name.compareIgnoreCase (b.name) < 0; });
            ports = owner.engine.getOutputNames();

            c.addItem ("Aucune sortie", 1);
            c.addSectionHeading ("Instruments virtuels");
            for (int i = 0; i < instruments.size(); ++i)
                c.addItem (instruments[i].name + "  (" + instruments[i].pluginFormatName + ")", 100 + i);
            c.addItem ("Chercher des instruments...", 2);
            c.addSectionHeading ("Ports MIDI");
            for (int i = 0; i < ports.size(); ++i)
                c.addItem (ports[i] == AudioEngine::virtualPortName() ? ports[i] + " (port virtuel)" : ports[i], 1000 + i);
            c.setSelectedId (0, juce::dontSendNotification);
        };
        output.onChange = [this]
        {
            const int id = output.getSelectedId();
            if (id == 1)
                owner.engine.clearTrackOutput (index);
            else if (id == 2)
            {
                if (owner.onScanPlugins) owner.onScanPlugins();
            }
            else if (id >= 1000 && id - 1000 < ports.size())
                owner.engine.setTrackMidiPort (index, ports[id - 1000], info().outputChannel);
            else if (id >= 100 && id - 100 < instruments.size())
                owner.engine.setTrackPlugin (index, instruments.getReference (id - 100));
            refreshDisplay = true;
        };

        outChannel.onFill = [this] (LazyCombo& c)
        {
            c.clear (juce::dontSendNotification);
            for (int ch = 1; ch <= 16; ++ch)
                c.addItem (channelName (ch), ch);
            c.setSelectedId (info().outputChannel, juce::dontSendNotification);
        };
        outChannel.onChange = [this]
        {
            if (outChannel.getSelectedId() > 0)
                owner.engine.setTrackOutputChannel (index, outChannel.getSelectedId());
        };

        edit.onClick = [this] { if (owner.onOpenPlugin) owner.onOpenPlugin (index); };
        arm.onClick = [this]
        {
            const bool exclusive = ! juce::ModifierKeys::currentModifiers.isCommandDown();
            owner.engine.edit ([&] (Session& s)
            {
                // Comme Ableton : armer une piste désarme les autres (Cmd pour en armer plusieurs).
                const bool now = ! trackAt (s).arm;
                if (exclusive && now)
                    for (auto& t : s.song.tracks) t.arm = false;
                trackAt (s).arm = now;
            });
        };
        mute.onClick = [this] { owner.engine.edit ([&] (Session& s) { auto& t = trackAt (s); t.mute = ! t.mute; }); };
        solo.onClick = [this]
        {
            const bool exclusive = ! juce::ModifierKeys::currentModifiers.isCommandDown();
            owner.engine.edit ([&] (Session& s)
            {
                const bool now = ! trackAt (s).solo;
                if (exclusive && now)
                    for (auto& t : s.song.tracks) t.solo = false;
                trackAt (s).solo = now;
            });
        };
        volume.onValueChange = [this]
        {
            const float v = (float) volume.getValue();
            owner.engine.edit ([&] (Session& s) { trackAt (s).volume = v; });
        };
    }

    // La piste de cette tranche (une piste fantôme si elle vient d'être supprimée).
    Track& trackAt (Session& s) const
    {
        static Track removed;
        return juce::isPositiveAndBelow (index, s.song.numTracks()) ? s.song.tracks[(size_t) index] : removed;
    }

    const SessionSnapshot::TrackInfo& info() const { return owner.view.tracks[(size_t) index]; }

    void refresh()
    {
        const auto& t = info();
        auto setText = [] (juce::ComboBox& c, const juce::String& text)
        {
            if (! c.isPopupActive() && c.getText() != text)
                c.setText (text, juce::dontSendNotification);
        };
        setText (input, ! t.inputEnabled ? juce::String::fromUTF8 ("Aucune entrée") : t.inputDevice.isEmpty() ? "Tous les claviers" : t.inputDevice);
        setText (inChannel, t.inputChannel == 0 ? "Tous canaux" : channelName (t.inputChannel));
        setText (output, t.outputType == OutputType::plugin ? t.pluginName
                       : t.outputType == OutputType::midiPort ? t.outputPort : juce::String ("Aucune sortie"));
        setText (outChannel, channelName (t.outputChannel));
        outChannel.setVisible (t.outputType == OutputType::midiPort);
        edit.setVisible (t.outputType == OutputType::plugin);
        arm.setToggleState (t.arm, juce::dontSendNotification);
        mute.setToggleState (t.mute, juce::dontSendNotification);
        solo.setToggleState (t.solo, juce::dontSendNotification);
        if (! volume.isMouseButtonDown() && std::abs (volume.getValue() - t.volume) > 1.0e-4)
            volume.setValue (t.volume, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::panel);
        g.fillRect (getLocalBounds().reduced (1, 0));
        g.setColour (info().colour);
        g.fillRect (1, 0, getWidth() - 2, 3);
        g.setColour (theme::textDim);
        g.setFont (font (10.0f, true));
        g.drawText (juce::String::fromUTF8 ("ENTRÉE"), 6, 6, 60, 12, juce::Justification::left);
        g.drawText ("SORTIE", 6, 58, 60, 12, juce::Justification::left);
    }

    void resized() override
    {
        const int w = getWidth() - 10;
        input.setBounds (5, 19, w, 20);
        inChannel.setBounds (5, 41, w, 18);
        output.setBounds (5, 71, w, 20);
        outChannel.setBounds (5, 93, w, 18);
        edit.setBounds (5, 93, w, 18);
        const int bw = (w - 8) / 3;
        arm.setBounds (5, 117, bw, 20);
        mute.setBounds (5 + bw + 4, 117, bw, 20);
        solo.setBounds (5 + 2 * (bw + 4), 117, bw, 20);
        volume.setBounds (3, 142, w + 4, 20);
    }

    SessionView& owner;
    int index = 0;
    LazyCombo input, inChannel, output, outChannel;
    juce::TextButton edit { "Ouvrir" }, arm { juce::String::fromUTF8 ("●") }, mute { "M" }, solo { "S" };
    juce::Slider volume;
    juce::StringArray inputNames, ports;
    juce::Array<juce::PluginDescription> instruments;
    bool refreshDisplay = false;
};

//==============================================================================
class SessionView::MasterStrip : public juce::Component
{
public:
    MasterStrip (SessionView& o) : owner (o)
    {
        addAndMakeVisible (volume);
        volume.setSliderStyle (juce::Slider::LinearHorizontal);
        volume.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        volume.setRange (0.0, 1.0);
        volume.setDoubleClickReturnValue (true, 0.85);
        volume.textFromValueFunction = [] (double v)
        {
            const auto db = volumeToDecibels ((float) v);
            return db <= -69.0f ? juce::String ("-inf") : juce::String (db, 1) + " dB";
        };
        volume.onValueChange = [this]
        {
            const float v = (float) volume.getValue();
            owner.engine.edit ([&] (Session& s) { s.song.masterVolume = v; });
        };
        volume.setTooltip (juce::String::fromUTF8 ("Volume général des instruments virtuels"));
    }

    void refresh()
    {
        if (! volume.isMouseButtonDown() && std::abs (volume.getValue() - owner.view.master) > 1.0e-4)
            volume.setValue (owner.view.master, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::panel);
        g.fillRect (getLocalBounds().reduced (1, 0));
        g.setColour (theme::accent);
        g.fillRect (1, 0, getWidth() - 2, 3);
        g.setColour (theme::text);
        g.setFont (font (13.0f, true));
        g.drawText ("Master", 8, 8, getWidth() - 16, 18, juce::Justification::left);
        g.setColour (theme::textDim);
        g.setFont (font (11.0f));
        g.drawFittedText ("Clic sur une case : lancer\nDouble-clic sur une case vide : nouveau clip\nGlisser un .mid : importer",
                          8, 32, getWidth() - 16, 70, juce::Justification::topLeft, 4);
    }

    void resized() override { volume.setBounds (4, 142, getWidth() - 8, 20); }

    SessionView& owner;
    juce::Slider volume;
};

//==============================================================================
SessionView::SessionView (AudioEngine& e) : engine (e)
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (hBar);
    addAndMakeVisible (vBar);
    addAndMakeVisible (stripHolder);
    hBar.addListener (this);
    vBar.addListener (this);
    hBar.setAutoHide (false);
    vBar.setAutoHide (false);
    master = std::make_unique<MasterStrip> (*this);
    addAndMakeVisible (*master);
    takeSnapshot();
    updateStrips();
    startTimerHz (30);
}

SessionView::~SessionView()
{
    stopTimer();
}

//==============================================================================
void SessionView::takeSnapshot()
{
    engine.read ([this] (const Session& s)
    {
        const auto& song = s.song;
        view.playing = s.isPlaying();
        view.beat = s.getBeat();
        view.tempo = song.tempo;
        view.master = song.masterVolume;
        view.tracks.resize ((size_t) song.numTracks());
        view.scenes.resize ((size_t) song.numScenes());
        view.cells.resize ((size_t) (song.numTracks() * song.numScenes()));
        for (int t = 0; t < song.numTracks(); ++t)
        {
            const auto& tr = song.tracks[(size_t) t];
            auto& ti = view.tracks[(size_t) t];
            ti.name = tr.name;
            ti.colour = tr.colour;
            ti.arm = tr.arm;
            ti.mute = tr.mute;
            ti.solo = tr.solo;
            ti.active = s.isTrackActive (t);
            ti.progress = s.getClipProgress (t);
            ti.inputDevice = tr.inputDevice;
            ti.inputEnabled = tr.inputEnabled;
            ti.inputChannel = tr.inputChannel;
            ti.outputType = tr.outputType;
            ti.outputPort = tr.outputPort;
            ti.pluginName = tr.pluginName;
            ti.outputChannel = tr.outputChannel;
            ti.volume = tr.volume;
            for (int sc = 0; sc < song.numScenes(); ++sc)
            {
                auto& ci = view.cells[(size_t) (t * song.numScenes() + sc)];
                ci.state = s.getSlotState (t, sc);
                if (auto clip = tr.slots[(size_t) sc])
                {
                    ci.name = clip->name;
                    ci.colour = clip->colour;
                    ci.notes = (int) clip->notes.size();
                }
                else
                {
                    ci.name = {};
                    ci.notes = 0;
                }
            }
        }
        for (int sc = 0; sc < song.numScenes(); ++sc)
        {
            view.scenes[(size_t) sc].name = song.scenes[(size_t) sc].name;
            view.scenes[(size_t) sc].active = s.isSceneActive (sc);
        }
    });
    selTrack = juce::jlimit (0, juce::jmax (0, (int) view.tracks.size() - 1), selTrack);
    selScene = juce::jlimit (0, juce::jmax (0, (int) view.scenes.size() - 1), selScene);
}

void SessionView::updateStrips()
{
    const int n = (int) view.tracks.size();
    while (strips.size() > n)
        strips.removeLast();
    while (strips.size() < n)
    {
        auto* strip = strips.add (new TrackStrip (*this));
        stripHolder.addAndMakeVisible (strip);
    }
    for (int t = 0; t < n; ++t)
    {
        strips[t]->index = t;
        strips[t]->setBounds (t * trackW - scrollX, 0, trackW, mixerH);
        strips[t]->refresh();
        strips[t]->repaint (0, 0, trackW, 4);
    }
    master->refresh();
}

std::uint64_t SessionView::signature() const
{
    // Empreinte de tout ce qui est affiché : on ne redessine que si elle change (ou pendant la lecture).
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&h] (std::uint64_t v) { h ^= v; h *= 1099511628211ull; };
    auto mixFloat = [&mix] (float f) { std::uint32_t bits; std::memcpy (&bits, &f, sizeof bits); mix (bits); };
    for (const auto& t : view.tracks)
    {
        mix ((std::uint64_t) t.name.hashCode64());
        mix (t.colour.getARGB());
        mix ((t.arm ? 1u : 0u) | (t.mute ? 2u : 0u) | (t.solo ? 4u : 0u) | (t.active ? 8u : 0u) | (t.inputEnabled ? 16u : 0u));
        mix ((std::uint64_t) t.inputDevice.hashCode64());
        mix ((std::uint64_t) (t.inputChannel * 64 + t.outputChannel * 4 + (int) t.outputType));
        mix ((std::uint64_t) t.outputPort.hashCode64());
        mix ((std::uint64_t) t.pluginName.hashCode64());
        mixFloat (t.volume);
    }
    for (const auto& sc : view.scenes)
    {
        mix ((std::uint64_t) sc.name.hashCode64());
        mix (sc.active ? 1u : 0u);
    }
    for (const auto& c : view.cells)
    {
        mix ((std::uint64_t) c.state);
        mix ((std::uint64_t) c.name.hashCode64());
        mix (c.colour.getARGB());
        mix ((std::uint64_t) c.notes);
    }
    mixFloat (view.master);
    mix ((std::uint64_t) (selTrack * 4096 + selScene));
    mix ((std::uint64_t) ((dropTrack + 1) * 4096 + dropScene + 1));
    mix (hasKeyboardFocus (true) ? 1u : 0u);
    return h;
}

void SessionView::timerCallback()
{
    const auto before = std::make_pair (view.tracks.size(), view.scenes.size());
    takeSnapshot();
    if (before != std::make_pair (view.tracks.size(), view.scenes.size()))
        updateScrollBars();

    const auto sig = signature();
    if (sig != lastSignature)
        updateStrips();
    // Pendant la lecture, les barres d'avancement et les clignotements bougent sans cesse.
    if (sig != lastSignature || view.playing)
        repaint (0, 0, getWidth(), getHeight() - mixerH - barT);
    lastSignature = sig;
}

void SessionView::updateScrollBars()
{
    const int contentW = ((int) view.tracks.size() + 1) * trackW;
    const int contentH = ((int) view.scenes.size() + 1) * slotH;
    const int visibleW = juce::jmax (1, gridRight());
    const int visibleH = juce::jmax (1, slotsBottom() - headerH);
    scrollX = juce::jlimit (0, juce::jmax (0, contentW - visibleW), scrollX);
    scrollY = juce::jlimit (0, juce::jmax (0, contentH - visibleH), scrollY);
    hBar.setRangeLimits (0, juce::jmax (contentW, visibleW), juce::dontSendNotification);
    hBar.setCurrentRange (scrollX, visibleW, juce::dontSendNotification);
    vBar.setRangeLimits (0, juce::jmax (contentH, visibleH), juce::dontSendNotification);
    vBar.setCurrentRange (scrollY, visibleH, juce::dontSendNotification);
}

void SessionView::scrollBarMoved (juce::ScrollBar* bar, double start)
{
    if (bar == &hBar) scrollX = (int) start;
    else scrollY = (int) start;
    updateStrips();
    repaint();
}

void SessionView::resized()
{
    hBar.setBounds (0, getHeight() - barT, gridRight(), barT);
    vBar.setBounds (getWidth() - barT, headerH, barT, slotsBottom() - headerH);
    stripHolder.setBounds (0, slotsBottom() + stopH, gridRight(), mixerH);
    master->setBounds (gridRight(), slotsBottom() + stopH, sceneW + barT, mixerH);
    updateScrollBars();
    updateStrips();
}

//==============================================================================
juce::Rectangle<int> SessionView::slotBounds (int track, int scene) const
{
    return { track * trackW - scrollX + 1, headerH + scene * slotH - scrollY + 1, trackW - 2, slotH - 2 };
}

void SessionView::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    const int nT = (int) view.tracks.size(), nS = (int) view.scenes.size();
    const int right = gridRight(), bottom = slotsBottom();
    const bool blinkOn = view.playing ? std::fmod (view.beat, 1.0) < 0.5 : (juce::Time::getMillisecondCounter() / 250) % 2 == 0;

    // --- Cases ---
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (0, headerH, right, bottom - headerH);
        for (int t = 0; t < nT; ++t)
        {
            const auto& tr = view.tracks[(size_t) t];
            for (int s = 0; s < nS; ++s)
            {
                const auto r = slotBounds (t, s);
                if (r.getBottom() < headerH || r.getY() > bottom || r.getRight() < 0 || r.getX() > right)
                    continue;
                const auto& c = view.cell (t, s);
                const auto button = r.withWidth (20).toFloat();

                if (c.state == SlotState::empty)
                {
                    g.setColour (theme::slot);
                    g.fillRect (r);
                    if (tr.arm)
                    {
                        g.setColour (theme::record.withAlpha (0.55f));
                        g.fillEllipse (button.withSizeKeepingCentre (8, 8));
                    }
                    else
                    {
                        g.setColour (juce::Colour (0xff4a4a4a));
                        g.fillRect (button.withSizeKeepingCentre (7, 7));
                    }
                }
                else
                {
                    const bool playing = c.state == SlotState::playing || c.state == SlotState::recording;
                    g.setColour (playing ? c.colour : c.colour.withMultipliedBrightness (0.82f));
                    g.fillRect (r);
                    if (playing)
                    {
                        // Avancement dans la boucle.
                        g.setColour (juce::Colours::black.withAlpha (0.25f));
                        g.fillRect (r.getX(), r.getBottom() - 3, (int) (r.getWidth() * tr.progress), 3);
                    }
                    juce::Colour mark = juce::Colours::black.withAlpha (0.55f);
                    if (c.state == SlotState::recording || c.state == SlotState::recordQueued)
                    {
                        g.setColour (juce::Colours::black.withAlpha (0.35f));
                        g.fillEllipse (button.withSizeKeepingCentre (12, 12));
                        if (c.state == SlotState::recording || blinkOn)
                        {
                            g.setColour (theme::record);
                            g.fillEllipse (button.withSizeKeepingCentre (9, 9));
                        }
                    }
                    else
                    {
                        if (c.state == SlotState::playing) mark = juce::Colours::black;
                        drawTriangle (g, button, mark);
                        if (c.state == SlotState::playing || (c.state == SlotState::playQueued && blinkOn))
                            drawTriangle (g, button.reduced (1.5f), theme::play);
                    }
                    g.setColour (juce::Colours::black.withAlpha (0.85f));
                    g.setFont (font (12.5f));
                    g.drawText (c.name.isNotEmpty() ? c.name : juce::String ("Clip"),
                                r.withTrimmedLeft (20).withTrimmedRight (4), juce::Justification::centredLeft, true);
                }

                if (c.state == SlotState::recordQueued && blinkOn && c.name.isEmpty())
                {
                    g.setColour (theme::record);
                    g.fillEllipse (button.withSizeKeepingCentre (9, 9));
                }

                if (t == selTrack && s == selScene && hasKeyboardFocus (true))
                {
                    g.setColour (theme::selection);
                    g.drawRect (r.expanded (1), 2);
                }
                else if (t == selTrack && s == selScene)
                {
                    g.setColour (theme::selection.withAlpha (0.5f));
                    g.drawRect (r.expanded (1), 1);
                }
                if (t == dropTrack && s == dropScene)
                {
                    g.setColour (theme::accent);
                    g.drawRect (r.expanded (1), 2);
                }
            }
            // Ligne « + » sous la dernière scène : rien, la colonne des scènes s'en charge.
        }
        // Colonne « + » : nouvelle piste.
        const int addX = nT * trackW - scrollX;
        g.setColour (theme::textDim.withAlpha (0.08f));
        g.fillRect (addX + 1, headerH, trackW - 2, nS * slotH - scrollY);
    }

    // --- Têtes de pistes ---
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (0, 0, right, headerH);
        for (int t = 0; t <= nT; ++t)
        {
            const juce::Rectangle<int> r (t * trackW - scrollX + 1, 1, trackW - 2, headerH - 2);
            if (t == nT)
            {
                g.setColour (theme::panel);
                g.fillRect (r);
                g.setColour (theme::textDim);
                g.setFont (font (15.0f, true));
                g.drawText ("+", r, juce::Justification::centred);
                continue;
            }
            const auto& tr = view.tracks[(size_t) t];
            g.setColour (t == selTrack ? tr.colour : tr.colour.withMultipliedBrightness (0.8f));
            g.fillRect (r);
            g.setColour (juce::Colours::black.withAlpha (0.85f));
            g.setFont (font (12.5f, true));
            g.drawText (tr.name, r.reduced (6, 0), juce::Justification::centredLeft, true);
        }
    }

    // --- Boutons d'arrêt des pistes ---
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (0, bottom, right, stopH);
        g.setColour (theme::panel);
        g.fillRect (0, bottom, right, stopH);
        for (int t = 0; t < nT; ++t)
        {
            const juce::Rectangle<int> r (t * trackW - scrollX + 1, bottom + 4, trackW - 2, stopH - 8);
            const bool active = view.tracks[(size_t) t].active;
            g.setColour (active ? theme::panelLight.brighter (0.15f) : theme::panelLight);
            g.fillRect (r);
            g.setColour (active ? theme::text : theme::textDim);
            g.fillRect (r.toFloat().withSizeKeepingCentre (8, 8));
        }
    }

    // --- Colonne des scènes ---
    {
        const int x = right;
        g.setColour (theme::panel);
        g.fillRect (x, 0, sceneW, bottom + stopH);
        g.setColour (theme::text);
        g.setFont (font (12.5f, true));
        g.drawText (juce::String::fromUTF8 ("Scènes"), x + 8, 0, sceneW - 16, headerH, juce::Justification::centredLeft);

        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (x, headerH, sceneW, bottom - headerH);
        for (int s = 0; s <= nS; ++s)
        {
            const juce::Rectangle<int> r (x + 1, headerH + s * slotH - scrollY + 1, sceneW - 2, slotH - 2);
            if (s == nS)
            {
                g.setColour (theme::textDim);
                g.setFont (font (15.0f, true));
                g.drawText ("+", r, juce::Justification::centred);
                continue;
            }
            const auto& sc = view.scenes[(size_t) s];
            g.setColour (sc.active ? juce::Colour (0xff3d4a36) : theme::panelLight);
            g.fillRect (r);
            drawTriangle (g, r.withWidth (22).toFloat(), sc.active ? theme::play : theme::textDim);
            g.setColour (theme::text);
            g.setFont (font (12.5f));
            g.drawText (sc.name, r.withTrimmedLeft (24), juce::Justification::centredLeft, true);
        }
    }
    {
        const juce::Rectangle<int> r (right + 1, bottom + 4, sceneW - 2, stopH - 8);
        g.setColour (theme::panelLight);
        g.fillRect (r);
        g.setColour (theme::text);
        g.fillRect (juce::Rectangle<float> ((float) r.getX() + 8.0f, (float) r.getCentreY() - 4.0f, 8.0f, 8.0f));
        g.setFont (font (12.0f));
        g.drawText (juce::String::fromUTF8 ("Tout arrêter"), r.withTrimmedLeft (24), juce::Justification::centredLeft);
    }

    // Séparation avec la console.
    g.setColour (theme::line);
    g.fillRect (0, bottom + stopH - 1, getWidth(), 1);
}

//==============================================================================
SessionView::Hit SessionView::findHit (juce::Point<int> p) const
{
    Hit h;
    const int nT = (int) view.tracks.size(), nS = (int) view.scenes.size();
    const int right = gridRight(), bottom = slotsBottom();

    if (p.x < right)
    {
        const int t = (p.x + scrollX) / trackW;
        const int localX = (p.x + scrollX) - t * trackW;
        if (p.y < headerH)
        {
            h.kind = t < nT ? Hit::Kind::header : t == nT ? Hit::Kind::addTrack : Hit::Kind::none;
            h.track = t;
        }
        else if (p.y < bottom)
        {
            const int s = (p.y - headerH + scrollY) / slotH;
            if (t < nT && s < nS)
            {
                h.kind = localX < 21 ? Hit::Kind::slotButton : Hit::Kind::slot;
                h.track = t;
                h.scene = s;
            }
            else if (t == nT && s < nS)
                h.kind = Hit::Kind::addTrack;
        }
        else if (p.y < bottom + stopH && t < nT)
        {
            h.kind = Hit::Kind::trackStop;
            h.track = t;
        }
    }
    else if (p.x < right + sceneW)
    {
        if (p.y >= headerH && p.y < bottom)
        {
            const int s = (p.y - headerH + scrollY) / slotH;
            if (s < nS)
            {
                h.kind = p.x - right < 24 ? Hit::Kind::sceneButton : Hit::Kind::scene;
                h.scene = s;
            }
            else if (s == nS)
                h.kind = Hit::Kind::addScene;
        }
        else if (p.y >= bottom && p.y < bottom + stopH)
            h.kind = Hit::Kind::stopAll;
    }
    return h;
}

void SessionView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto h = findHit (e.getPosition());
    const bool popup = e.mods.isPopupMenu();
    using K = Hit::Kind;

    switch (h.kind)
    {
        case K::header:
            selTrack = h.track;
            if (popup) showTrackMenu (h.track);
            break;
        case K::addTrack:
            engine.insertTrack ((int) view.tracks.size());
            break;
        case K::slotButton:
            selectCell (h.track, h.scene);
            if (popup) { showSlotMenu (h.track, h.scene); break; }
            engine.perform ([&] (Session& s) { s.launchSlot (h.track, h.scene); });
            break;
        case K::slot:
            selectCell (h.track, h.scene);
            if (popup) { showSlotMenu (h.track, h.scene); break; }
            if (view.cell (h.track, h.scene).state != SlotState::empty)
            {
                dragTrack = h.track;
                dragScene = h.scene;
                dragStart = e.getPosition();
            }
            break;
        case K::addScene:
            insertScene();
            break;
        case K::sceneButton:
            if (popup) { showSceneMenu (h.scene); break; }
            engine.perform ([&] (Session& s) { s.launchScene (h.scene); });
            break;
        case K::scene:
            selScene = h.scene;
            if (popup) showSceneMenu (h.scene);
            break;
        case K::trackStop:
            engine.perform ([&] (Session& s) { s.stopTrack (h.track); });
            break;
        case K::stopAll:
            engine.perform ([] (Session& s) { s.stopAllClips(); });
            break;
        case K::none:
            break;
    }
    repaint();
}

void SessionView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragTrack < 0)
        return;
    if (! dragging && e.getPosition().getDistanceFrom (dragStart) < 5)
        return;
    dragging = true;
    dragCopy = e.mods.isAltDown() || e.mods.isCommandDown();
    const auto h = findHit (e.getPosition());
    if (h.kind == Hit::Kind::slot || h.kind == Hit::Kind::slotButton)
    {
        dropTrack = h.track;
        dropScene = h.scene;
    }
    else
        dropTrack = dropScene = -1;
    setMouseCursor (dragCopy ? juce::MouseCursor::CopyingCursor : juce::MouseCursor::DraggingHandCursor);
    repaint();
}

void SessionView::mouseUp (const juce::MouseEvent&)
{
    if (dragging && dropTrack >= 0 && (dropTrack != dragTrack || dropScene != dragScene))
    {
        const int ft = dragTrack, fs = dragScene, tt = dropTrack, ts = dropScene;
        const bool copy = dragCopy;
        engine.edit ([&] (Session& s) { s.moveClip (ft, fs, tt, ts, copy); });
        selectCell (tt, ts);
    }
    dragging = false;
    dragTrack = dragScene = dropTrack = dropScene = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void SessionView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto h = findHit (e.getPosition());
    using K = Hit::Kind;
    if (h.kind == K::slot)
    {
        if (view.cell (h.track, h.scene).state == SlotState::empty)
            createEmptyClip (h.track, h.scene, 1);
        else if (onEditClip)
            onEditClip (h.track, h.scene);
    }
    else if (h.kind == K::header)
    {
        const int t = h.track;
        askText ("Nom de la piste", view.tracks[(size_t) t].name, [this, t] (const juce::String& name)
                 { engine.edit ([&] (Session& s) { if (t < s.song.numTracks()) s.song.tracks[(size_t) t].name = name; }); });
    }
    else if (h.kind == K::scene)
    {
        const int sc = h.scene;
        askText (juce::String::fromUTF8 ("Nom de la scène"), view.scenes[(size_t) sc].name, [this, sc] (const juce::String& name)
                 { engine.edit ([&] (Session& s) { if (sc < s.song.numScenes()) s.song.scenes[(size_t) sc].name = name; }); });
    }
}

void SessionView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const bool horizontal = e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY);
    const float delta = horizontal ? (std::abs (w.deltaX) > 0.0f ? w.deltaX : w.deltaY) : w.deltaY;
    if (horizontal) scrollX -= juce::roundToInt (delta * 200.0f);
    else scrollY -= juce::roundToInt (delta * 200.0f);
    updateScrollBars();
    updateStrips();
    repaint();
}

bool SessionView::keyPressed (const juce::KeyPress& key)
{
    const int nT = (int) view.tracks.size(), nS = (int) view.scenes.size();
    if (key == juce::KeyPress::leftKey)  { selectCell (juce::jmax (0, selTrack - 1), selScene); return true; }
    if (key == juce::KeyPress::rightKey) { selectCell (juce::jmin (nT - 1, selTrack + 1), selScene); return true; }
    if (key == juce::KeyPress::upKey)    { selectCell (selTrack, juce::jmax (0, selScene - 1)); return true; }
    if (key == juce::KeyPress::downKey)  { selectCell (selTrack, juce::jmin (nS - 1, selScene + 1)); return true; }
    if (key == juce::KeyPress::returnKey) { launchSelected(); return true; }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) { deleteSelected(); return true; }
    return false;
}

void SessionView::selectCell (int track, int scene)
{
    selTrack = juce::jlimit (0, juce::jmax (0, (int) view.tracks.size() - 1), track);
    selScene = juce::jlimit (0, juce::jmax (0, (int) view.scenes.size() - 1), scene);
    ensureVisible (selTrack, selScene);
    repaint();
}

void SessionView::ensureVisible (int track, int scene)
{
    const int x = track * trackW, y = scene * slotH;
    const int visibleW = gridRight(), visibleH = slotsBottom() - headerH;
    if (x < scrollX) scrollX = x;
    else if (x + trackW > scrollX + visibleW) scrollX = x + trackW - visibleW;
    if (y < scrollY) scrollY = y;
    else if (y + slotH > scrollY + visibleH) scrollY = y + slotH - visibleH;
    updateScrollBars();
    updateStrips();
}

//==============================================================================
// Commandes
bool SessionView::hasClipSelected() const
{
    return juce::isPositiveAndBelow (selTrack, (int) view.tracks.size()) && juce::isPositiveAndBelow (selScene, (int) view.scenes.size())
        && view.cell (selTrack, selScene).state != SlotState::empty;
}

void SessionView::launchSelected()
{
    const int t = selTrack, sc = selScene;
    engine.perform ([&] (Session& s) { s.launchSlot (t, sc); });
}

void SessionView::deleteSelected()
{
    const int t = selTrack, sc = selScene;
    engine.edit ([&] (Session& s) { s.setClip (t, sc, nullptr); });
}

void SessionView::duplicateSelected()
{
    // Comme Ableton : copie le clip dans la case libre suivante de la piste (en ajoutant une scène s'il le faut).
    const int t = selTrack, sc = selScene;
    int target = -1;
    engine.edit ([&] (Session& s)
    {
        auto clip = s.song.clipAt (t, sc);
        if (clip == nullptr)
            return;
        target = sc + 1;
        if (target >= s.song.numScenes() || s.song.clipAt (t, target) != nullptr)
        {
            s.insertScene (target, { juce::String (s.song.numScenes() + 1) });
        }
        s.setClip (t, target, clip->copy());
    });
    if (target >= 0)
        selectCell (t, target);
}

void SessionView::copySelected()
{
    clipboard = engine.read ([this] (const Session& s) { auto c = s.song.clipAt (selTrack, selScene); return c != nullptr ? c->copy() : nullptr; });
}

void SessionView::pasteSelected()
{
    if (clipboard == nullptr)
        return;
    const int t = selTrack, sc = selScene;
    auto clip = clipboard->copy();
    engine.edit ([&] (Session& s) { s.setClip (t, sc, clip); });
}

void SessionView::renameSelected()
{
    if (! hasClipSelected())
        return;
    const int t = selTrack, sc = selScene;
    askText ("Nom du clip", view.cell (t, sc).name, [this, t, sc] (const juce::String& name)
             { engine.edit ([&] (Session& s) { if (auto c = s.song.clipAt (t, sc)) c->name = name; }); });
}

void SessionView::insertTrack()
{
    engine.insertTrack (selTrack + 1);
    selTrack += 1;
}

void SessionView::insertScene()
{
    const int at = juce::jmin (selScene + 1, (int) view.scenes.size());
    engine.edit ([&] (Session& s) { s.insertScene (at, { juce::String (s.song.numScenes() + 1) }); });
    selScene = at;
}

void SessionView::createEmptyClip (int track, int scene, int bars)
{
    engine.edit ([&] (Session& s)
    {
        auto clip = std::make_shared<Clip>();
        clip->name = s.song.tracks[(size_t) track].name;
        clip->colour = s.song.tracks[(size_t) track].colour;
        clip->length = bars * beatsPerBar;
        s.setClip (track, scene, clip);
    });
    selectCell (track, scene);
}

//==============================================================================
// Menus
void SessionView::showSlotMenu (int track, int scene)
{
    juce::PopupMenu m;
    const bool hasClip = view.cell (track, scene).state != SlotState::empty;
    enum { launch = 1, rename, duplicate, copy, paste, remove, exportMidi, importMidi, edit, new1 = 50, new2, new4, new8, colourBase = 100 };

    if (hasClip)
    {
        m.addItem (launch, "Lancer", true, false);
        m.addItem (edit, juce::CharPointer_UTF8 ("Éditer les notes (double-clic)"), onEditClip != nullptr);
        m.addSeparator();
        m.addItem (rename, "Renommer");
        juce::PopupMenu colours;
        for (size_t i = 0; i < palette().size(); ++i)
            colours.addColouredItem (colourBase + (int) i, juce::String::fromUTF8 (colourNames[i]), palette()[i]);
        m.addSubMenu ("Couleur", colours);
        m.addItem (duplicate, "Dupliquer");
        m.addItem (copy, "Copier");
        m.addItem (paste, "Coller", clipboard != nullptr);
        m.addItem (remove, "Supprimer");
        m.addSeparator();
        m.addItem (exportMidi, "Exporter en fichier MIDI...");
    }
    else
    {
        m.addItem (new1, "Nouveau clip : 1 mesure");
        m.addItem (new2, "Nouveau clip : 2 mesures");
        m.addItem (new4, "Nouveau clip : 4 mesures");
        m.addItem (new8, "Nouveau clip : 8 mesures");
        m.addSeparator();
        m.addItem (paste, "Coller", clipboard != nullptr);
        m.addItem (importMidi, "Importer un fichier MIDI...");
    }

    m.showMenuAsync (juce::PopupMenu::Options(), [this, track, scene] (int r)
    {
        selectCell (track, scene);
        switch (r)
        {
            case launch:     launchSelected(); break;
            case edit:       if (onEditClip) onEditClip (track, scene); break;
            case rename:     renameSelected(); break;
            case duplicate:  duplicateSelected(); break;
            case copy:       copySelected(); break;
            case paste:      pasteSelected(); break;
            case remove:     deleteSelected(); break;
            case exportMidi: exportClip (track, scene); break;
            case new1:       createEmptyClip (track, scene, 1); break;
            case new2:       createEmptyClip (track, scene, 2); break;
            case new4:       createEmptyClip (track, scene, 4); break;
            case new8:       createEmptyClip (track, scene, 8); break;
            case importMidi:
                chooser = std::make_unique<juce::FileChooser> ("Importer un fichier MIDI", juce::File(), "*.mid;*.midi");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [this, track, scene] (const juce::FileChooser& fc)
                                      {
                                          if (fc.getResult().existsAsFile())
                                              importMidiFiles ({ fc.getResult().getFullPathName() }, track, scene);
                                      });
                break;
            default:
                if (r >= colourBase && r < colourBase + (int) palette().size())
                {
                    const auto colour = palette()[(size_t) (r - colourBase)];
                    engine.edit ([&] (Session& s) { if (auto c = s.song.clipAt (track, scene)) c->colour = colour; });
                }
                break;
        }
    });
}

void SessionView::showTrackMenu (int track)
{
    enum { rename = 1, insertBefore, insertAfter, remove, colourBase = 100 };
    juce::PopupMenu m;
    m.addItem (rename, "Renommer");
    juce::PopupMenu colours;
    for (size_t i = 0; i < palette().size(); ++i)
        colours.addColouredItem (colourBase + (int) i, juce::String::fromUTF8 (colourNames[i]), palette()[i]);
    m.addSubMenu ("Couleur", colours);
    m.addSeparator();
    m.addItem (insertBefore, juce::String::fromUTF8 ("Insérer une piste avant"));
    m.addItem (insertAfter, juce::String::fromUTF8 ("Insérer une piste après"));
    m.addItem (remove, "Supprimer la piste", view.tracks.size() > 1);

    m.showMenuAsync (juce::PopupMenu::Options(), [this, track] (int r)
    {
        if (r == rename)
            askText ("Nom de la piste", view.tracks[(size_t) track].name, [this, track] (const juce::String& name)
                     { engine.edit ([&] (Session& s) { if (track < s.song.numTracks()) s.song.tracks[(size_t) track].name = name; }); });
        else if (r == insertBefore) engine.insertTrack (track);
        else if (r == insertAfter)  engine.insertTrack (track + 1);
        else if (r == remove)
        {
            juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                                   .withIconType (juce::MessageBoxIconType::WarningIcon)
                                                   .withTitle ("Supprimer la piste")
                                                   .withMessage (juce::String::fromUTF8 ("Supprimer « ") + view.tracks[(size_t) track].name + juce::String::fromUTF8 (" » et tous ses clips ?"))
                                                   .withButton ("Supprimer")
                                                   .withButton ("Annuler"),
                                               [this, track] (int result) { if (result == 0) engine.removeTrack (track); });
        }
        else if (r >= colourBase && r < colourBase + (int) palette().size())
        {
            const auto colour = palette()[(size_t) (r - colourBase)];
            engine.edit ([&] (Session& s) { if (track < s.song.numTracks()) s.song.tracks[(size_t) track].colour = colour; });
        }
    });
}

void SessionView::showSceneMenu (int scene)
{
    enum { launch = 1, rename, insertBefore, insertAfter, remove };
    juce::PopupMenu m;
    m.addItem (launch, juce::String::fromUTF8 ("Lancer la scène"));
    m.addItem (rename, "Renommer");
    m.addSeparator();
    m.addItem (insertBefore, juce::String::fromUTF8 ("Insérer une scène avant"));
    m.addItem (insertAfter, juce::String::fromUTF8 ("Insérer une scène après"));
    m.addItem (remove, juce::String::fromUTF8 ("Supprimer la scène"), view.scenes.size() > 1);
    m.showMenuAsync (juce::PopupMenu::Options(), [this, scene] (int r)
    {
        if (r == launch) engine.perform ([&] (Session& s) { s.launchScene (scene); });
        else if (r == rename)
            askText (juce::String::fromUTF8 ("Nom de la scène"), view.scenes[(size_t) scene].name, [this, scene] (const juce::String& name)
                     { engine.edit ([&] (Session& s) { if (scene < s.song.numScenes()) s.song.scenes[(size_t) scene].name = name; }); });
        else if (r == insertBefore) engine.edit ([&] (Session& s) { s.insertScene (scene, { juce::String (s.song.numScenes() + 1) }); });
        else if (r == insertAfter)  engine.edit ([&] (Session& s) { s.insertScene (scene + 1, { juce::String (s.song.numScenes() + 1) }); });
        else if (r == remove)       engine.edit ([&] (Session& s) { s.removeScene (scene); });
    });
}

void SessionView::askText (const juce::String& title, const juce::String& initial, std::function<void (const juce::String&)> done)
{
    auto* w = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("text", initial);
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    if (auto* editor = w->getTextEditor ("text"))
        editor->selectAll();
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, done] (int result)
    {
        const auto text = w->getTextEditorContents ("text").trim();
        if (result == 1 && text.isNotEmpty())
            done (text);
    }), true);
}

//==============================================================================
// Fichiers MIDI
bool SessionView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"))
            return true;
    return false;
}

void SessionView::fileDragMove (const juce::StringArray&, int x, int y)
{
    const auto h = findHit ({ x, y });
    const bool onSlot = h.kind == Hit::Kind::slot || h.kind == Hit::Kind::slotButton;
    dropTrack = onSlot ? h.track : -1;
    dropScene = onSlot ? h.scene : -1;
    repaint();
}

void SessionView::fileDragExit (const juce::StringArray&)
{
    dropTrack = dropScene = -1;
    repaint();
}

void SessionView::filesDropped (const juce::StringArray& files, int x, int y)
{
    const auto h = findHit ({ x, y });
    dropTrack = dropScene = -1;
    if (h.kind == Hit::Kind::slot || h.kind == Hit::Kind::slotButton)
        importMidiFiles (files, h.track, h.scene);
    repaint();
}

void SessionView::importMidiFiles (const juce::StringArray& files, int track, int scene)
{
    // Plusieurs fichiers : un par case, en descendant (des scènes s'ajoutent s'il le faut).
    int s = scene;
    for (const auto& path : files)
    {
        auto clip = clipFromMidiFile (juce::File (path));
        if (clip == nullptr)
            continue;
        engine.edit ([&] (Session& session)
        {
            clip->colour = session.song.tracks[(size_t) track].colour;
            while (s >= session.song.numScenes())
                session.insertScene (session.song.numScenes(), { juce::String (session.song.numScenes() + 1) });
            session.setClip (track, s, clip);
        });
        ++s;
    }
    selectCell (track, scene);
}

void SessionView::exportClip (int track, int scene)
{
    auto data = engine.read ([&] (const Session& s)
    {
        auto c = s.song.clipAt (track, scene);
        return std::make_pair (c != nullptr ? c->copy() : nullptr, s.song.tempo);
    });
    if (data.first == nullptr)
        return;
    const auto name = data.first->name.isNotEmpty() ? data.first->name : juce::String ("Clip");
    chooser = std::make_unique<juce::FileChooser> ("Exporter en fichier MIDI",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (name + ".mid"),
                                                   "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [data] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file != juce::File())
                                  clipToMidiFile (*data.first, data.second, file.withFileExtension ("mid"));
                          });
}
} // namespace gridplay
