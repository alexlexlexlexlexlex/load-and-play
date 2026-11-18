#include "PluginEditor.h"

namespace
{
    void styleTextBox (juce::Slider& slider);

    // Palette de la façade du Juno-60 : panneau noir, bandeaux bleus, rouges
    // et blancs, boutons crème / jaune / orange à LED rouge, bandeau alu.
    namespace Colours
    {
        const juce::Colour panel     { 0xff161616 };
        const juce::Colour text      { 0xfff0ede4 };
        const juce::Colour dimText   { 0xffa39e94 };
        const juce::Colour slot      { 0xff000000 };
        const juce::Colour ticks     { 0xff8a8780 };
        const juce::Colour cap       { 0xffd8d6d0 };
        const juce::Colour ledOn     { 0xffff3326 };
        const juce::Colour ledOff    { 0xff4a1512 };
        const juce::Colour woodDark  { 0xff4a2a14 };
        const juce::Colour woodLight { 0xff7b4a26 };
        const juce::Colour silver    { 0xffbdbcb7 };

        const juce::Colour bandWhite { 0xffe6e3da };
        const juce::Colour bandBlue  { 0xff3e5fab };
        const juce::Colour bandRed   { 0xffc9352d };

        const juce::Colour keys[] = { juce::Colour (0xffe9dfc2), juce::Colour (0xffe9c440), juce::Colour (0xffe8862c) };
        const juce::Colour accent    { 0xffe8862c };
    }

    constexpr int woodWidth = 22;
    constexpr int left = woodWidth + 12;
    constexpr int columnWidth = 44;
    constexpr int sectionGap = 14;
    constexpr int grilleHeight = 18;
    constexpr int headerHeight = 76;
    constexpr int bandHeight = 22;
    constexpr int controlsTop = headerHeight + bandHeight + 4;
    constexpr int controlsHeight = 184;
    constexpr int columnTitle = 15;
    constexpr int bottomTop = controlsTop + controlsHeight + 10;
    constexpr int bottomHeight = 104;
    constexpr int designHeight = bottomTop + bottomHeight;
    constexpr int knobWidth = 54;
    constexpr int wheelsWidth = 64;
    constexpr int playerWidth = 190;

    // Valeur affichée sous le réglage, discrète et sans cadre.
    void styleTextBox (juce::Slider& slider)
    {
        slider.setColour (juce::Slider::textBoxTextColourId, Colours::dimText);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }
}

//==============================================================================
PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::dimText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    // Afficheur de preset : texte rouge sur fond noir, comme l'écran du patch.
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff070707));
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff4a4a4a));
    setColour (juce::ComboBox::textColourId, Colours::ledOn);
    setColour (juce::ComboBox::arrowColourId, Colours::dimText);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1a1a));
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff333333));
    setColour (juce::TextButton::textColourOffId, Colours::text);
}

void PanelLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float, float, juce::Slider::SliderStyle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();

    if (slider.getProperties()["wheel"])
    {
        // Molette : fente sombre, roue grise striée, repère blanc.
        const auto slotArea = bounds.reduced (2.0f, 0.0f);
        g.setColour (Colours::slot);
        g.fillRoundedRectangle (slotArea, 4.0f);
        const auto wheel = slotArea.reduced (3.0f, 4.0f);
        juce::ColourGradient shading (juce::Colour (0xff1e1e1e), 0.0f, wheel.getY(),
                                      juce::Colour (0xff1e1e1e), 0.0f, wheel.getBottom(), false);
        shading.addColour (0.5, juce::Colour (0xff6a6a6a));
        g.setGradientFill (shading);
        g.fillRoundedRectangle (wheel, 3.0f);
        g.setColour (Colours::text);
        g.fillRect (juce::Rectangle<float> (wheel.getWidth(), 3.0f).withCentre ({ wheel.getCentreX(), sliderPos }));
        return;
    }

    // Curseur vertical façon Juno : échelle en échelons des deux côtés de la
    // fente, capuchon gris clair à trait noir.
    const float cx = bounds.getCentreX();
    const float top = bounds.getY() + 8.0f, bottom = bounds.getBottom() - 8.0f;

    const bool stepped = slider.getInterval() >= 1.0;
    const int numTicks = stepped ? juce::roundToInt (slider.getRange().getLength()) + 1 : 21;
    for (int i = 0; i < numTicks; ++i)
    {
        const float ty = std::round (bottom - (bottom - top) * (float) i / (float) (numTicks - 1)) + 0.5f;
        const bool major = stepped || i % 10 == 0 || i == numTicks / 2;
        g.setColour (major ? Colours::text.withAlpha (0.85f) : Colours::ticks.withAlpha (0.7f));
        const float reach = major ? 15.0f : 12.0f;
        g.drawLine (cx - reach, ty, cx - 5.0f, ty, major ? 1.2f : 0.8f);
        g.drawLine (cx + 5.0f, ty, cx + reach, ty, major ? 1.2f : 0.8f);
    }

    g.setColour (Colours::slot);
    g.fillRect (juce::Rectangle<float> (3.0f, bottom - top + 8.0f).withCentre ({ cx, (top + bottom) * 0.5f }));

    const auto cap = juce::Rectangle<float> (20.0f, 11.0f).withCentre ({ cx, sliderPos });
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (cap.translated (0.0f, 1.5f));
    g.setGradientFill (juce::ColourGradient (Colours::cap.brighter (0.15f), cap.getX(), cap.getY(),
                                             Colours::cap.darker (0.25f), cap.getX(), cap.getBottom(), false));
    g.fillRect (cap);
    g.setColour (juce::Colours::black);
    g.fillRect (juce::Rectangle<float> (cap.getWidth(), 1.6f).withCentre (cap.getCentre()));
}

void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 7.0f;

    // Graduation en points autour du bouton, comme sur la façade.
    g.setColour (Colours::text.withAlpha (0.8f));
    for (int i = 0; i < 11; ++i)
    {
        const float angle = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        g.fillEllipse (juce::Rectangle<float> (2.2f, 2.2f).withCentre (centre.getPointOnCircumference (radius + 4.5f, angle)));
    }

    g.setColour (juce::Colour (0xff0c0c0c));
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
    g.setColour (juce::Colour (0xff3a3a3a));
    g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.5f);

    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    g.setColour (Colours::text);
    g.drawLine ({ centre.getPointOnCircumference (radius * 0.25f, angle),
                  centre.getPointOnCircumference (radius * 0.9f, angle) }, 2.0f);
}

void PanelLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down)
{
    // LED rouge au-dessus d'un bouton carré gris, nom en dessous.
    auto bounds = button.getLocalBounds().toFloat();
    const auto textArea = bounds.removeFromBottom (12.0f);
    const auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ bounds.getCentreX(), bounds.getY() + 5.0f });
    const auto key = juce::Rectangle<float> (24.0f, 16.0f).withCentre ({ bounds.getCentreX(), bounds.getY() + 21.0f });

    const bool on = button.getToggleState();
    if (on)
    {
        g.setColour (Colours::ledOn.withAlpha (0.3f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (on ? Colours::ledOn : Colours::ledOff);
    g.fillEllipse (led);

    const auto keyColour = juce::Colour ((juce::uint32) (juce::int64) button.getProperties()["key"]);
    const auto face = keyColour.brighter (highlighted ? 0.1f : 0.0f).darker (down ? 0.25f : 0.0f);
    g.setGradientFill (juce::ColourGradient (face.brighter (0.15f), key.getX(), key.getY(),
                                             face.darker (0.2f), key.getX(), key.getBottom(), false));
    g.fillRect (key);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRect (key, 1.0f);

    // Pictogramme de la forme d'onde, imprimé sur le bouton.
    const auto wave = button.getProperties()["wave"].toString();
    if (wave.isNotEmpty())
    {
        const auto r = key.reduced (5.0f, 4.0f);
        juce::Path icon;
        if (wave == "saw")
        {
            icon.startNewSubPath (r.getX(), r.getBottom());
            icon.lineTo (r.getCentreX(), r.getY());
            icon.lineTo (r.getCentreX(), r.getBottom());
            icon.lineTo (r.getRight(), r.getY());
            icon.lineTo (r.getRight(), r.getBottom());
        }
        else
        {
            // Carré ; le sous-oscillateur est dessiné une octave plus bas (cycle deux fois plus long).
            const float half = wave == "pulse" ? r.getWidth() / 4.0f : r.getWidth() / 2.0f;
            float x = r.getX();
            bool high = true;
            icon.startNewSubPath (x, r.getBottom());
            while (x < r.getRight() - 0.5f)
            {
                const float y = high ? r.getY() : r.getBottom();
                icon.lineTo (x, y);
                x = juce::jmin (r.getRight(), x + half);
                icon.lineTo (x, y);
                high = ! high;
            }
        }
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.strokePath (icon, juce::PathStrokeType (1.2f));
    }

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    g.drawFittedText (button.getButtonText(), textArea.toNearestInt(), juce::Justification::centred, 1);
}

//==============================================================================
LedChoice::LedChoice (juce::RangedAudioParameter& parameter, juce::StringArray choiceLabels)
    : labels (std::move (choiceLabels)),
      attachment (parameter, [this] (float value) { index = juce::roundToInt (value); repaint(); })
{
    attachment.sendInitialUpdate();
}

void LedChoice::paint (juce::Graphics& g)
{
    // Chaque position : LED rouge, petit bouton coloré, nom.
    const float rowHeight = juce::jmin (30.0f, (float) getHeight() / (float) labels.size());
    g.setFont (juce::FontOptions (8.5f, juce::Font::bold));

    for (int i = 0; i < labels.size(); ++i)
    {
        const auto row = juce::Rectangle<float> (0.0f, (float) i * rowHeight, (float) getWidth(), rowHeight);
        const auto led = juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ row.getX() + 8.0f, row.getY() + 5.0f });
        const auto key = juce::Rectangle<float> (13.0f, 10.0f).withCentre ({ row.getX() + 8.0f, row.getY() + 15.0f });
        const bool on = i == index;

        if (on)
        {
            g.setColour (Colours::ledOn.withAlpha (0.3f));
            g.fillEllipse (led.expanded (2.5f));
        }
        g.setColour (on ? Colours::ledOn : Colours::ledOff);
        g.fillEllipse (led);

        const auto colour = Colours::keys[i % 3];
        g.setGradientFill (juce::ColourGradient (colour.brighter (0.15f), key.getX(), key.getY(),
                                                 colour.darker (0.2f), key.getX(), key.getBottom(), false));
        g.fillRect (key);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRect (key, 1.0f);

        g.setColour (on ? Colours::text : Colours::dimText);
        g.drawText (labels[i], juce::Rectangle<float> (row.getX() + 17.0f, key.getY() - 2.0f, row.getWidth() - 17.0f, 14.0f),
                    juce::Justification::centredLeft);
    }
}

void LedChoice::mouseDown (const juce::MouseEvent& e)
{
    const float rowHeight = juce::jmin (30.0f, (float) getHeight() / (float) labels.size());
    const int clicked = (int) ((float) e.y / rowHeight);
    if (juce::isPositiveAndBelow (clicked, labels.size()))
        attachment.setValueAsCompleteGesture ((float) clicked);
}

//==============================================================================
LP60Editor::LP60Editor (LP60Processor& p)
    : AudioProcessorEditor (p),
      processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      playerPanel (p.midiPlayer, Colours::accent, Colours::text)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (panel);

    beginSection ("ARPEGGIO", Colours::bandWhite);
    addButtons ({ { "arpOn", "ON" }, { "hold", "HOLD" }, { "arpSync", "SYNC" } });
    addChoice ("arpMode", "MODE", { "UP", "U&D", "DOWN" });
    addChoice ("arpRange", "RANGE", { "1", "2", "3" });
    addSlider ("arpRate", "RATE");

    beginSection ("LFO", Colours::bandWhite);
    addSlider ("lfoRate", "RATE");
    addSlider ("lfoDelay", "DELAY");
    addChoice ("lfoMode", "TRIG", { "AUTO", "MAN" });

    beginSection ("DCO", Colours::bandBlue);
    addChoice ("range", "RANGE", { "16'", "8'", "4'" });
    addSlider ("dcoLfo", "LFO");
    addSlider ("pwm", "PWM");
    addChoice ("pwmMode", "PWM SRC", { "LFO", "MAN", "ENV" });
    addButtons ({ { "pulse", "PULSE" }, { "saw", "SAW" }, { "subOn", "SUB" } });
    addSlider ("sub", "SUB");
    addSlider ("noise", "NOISE");

    beginSection ("HPF", Colours::bandRed);
    addSlider ("hpf", "FREQ");

    beginSection ("VCF", Colours::bandRed);
    addSlider ("cutoff", "FREQ");
    addSlider ("resonance", "RES");
    addChoice ("envPolarity", "ENV", { "+", juce::String::fromUTF8 ("\xe2\x88\x92") });
    addSlider ("vcfEnv", "ENV");
    addSlider ("vcfLfo", "LFO");
    addSlider ("vcfKybd", "KYBD");

    beginSection ("VCA", Colours::bandRed);
    addChoice ("vcaMode", "MODE", { "ENV", "GATE" });
    addSlider ("level", "LEVEL");

    beginSection ("ENV", Colours::bandRed);
    addSlider ("attack", "A");
    addSlider ("decay", "D");
    addSlider ("sustain", "S");
    addSlider ("release", "R");

    beginSection ("CHORUS", Colours::bandBlue, 1);
    addButtons ({ { "chorus1", "I" } });
    addButtons ({ { "chorus2", "II" } });
    addSlider ("chorusHiss", "NOISE");

    // Réglages de jeu, à gauche du clavier.
    for (auto [id, title] : { std::pair<const char*, const char*> { "tune", "TUNE" }, { "bendDco", "BEND DCO" },
                              { "bendVcf", "BEND VCF" }, { "lfoMod", "MOD LFO" }, { "bpm", "TEMPO" } })
        knobs.push_back ({ &addKnob (id), title });

    // Presets
    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (juce::String (i + 1).paddedLeft ('0', 2) + "  " + processor.getProgramName (i), i + 1);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedItemIndex();
        if (index >= 0 && index != processor.getCurrentProgram())
            processor.setCurrentProgram (index);
    };
    previousPreset.onClick = [this] { processor.setCurrentProgram (juce::jmax (0, processor.getCurrentProgram() - 1)); };
    nextPreset.onClick = [this]
    {
        processor.setCurrentProgram (juce::jmin (processor.getNumPrograms() - 1, processor.getCurrentProgram() + 1));
    };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &previousPreset, &nextPreset })
        panel.addAndMakeVisible (c);
    refreshPresetBox();

    // Levier et molette
    for (auto* wheel : { &pitchWheel, &modWheel })
    {
        wheel->getProperties().set ("wheel", true);
        panel.addAndMakeVisible (wheel);
    }
    pitchWheel.setRange (-1.0, 1.0);
    pitchWheel.setValue (0.0, juce::dontSendNotification);
    pitchWheel.onValueChange = [this] { sendWheel (true, pitchWheel.getValue()); };
    pitchWheel.onDragEnd = [this] { pitchWheel.setValue (0.0); };
    modWheel.setRange (0.0, 1.0);
    modWheel.onValueChange = [this] { sendWheel (false, modWheel.getValue()); };

    keyboard.setAvailableRange (36, 96);   // 61 touches comme le Juno-60
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Colours::accent.withAlpha (0.7f));
    panel.addAndMakeVisible (keyboard);

    // Lecteur MIDI : invisible tant qu'aucun fichier n'est glissé sur la fenêtre.
    playerPanel.onClose = [this] { playerPanel.setVisible (false); resized(); };
    playerPanel.setVisible (processor.midiPlayer.isLoaded());
    playerPanel.refresh();
    panel.addChildComponent (playerPanel);

    const int width = columnArea ((int) columns.size() - 1).getRight() + left;
    panel.setSize (width, designHeight);

    setResizable (true, true);
    setResizeLimits (width / 2, designHeight / 2, width * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) width / (double) designHeight);

    float scale = 1.0f;
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        scale = juce::jmin (1.0f, 0.92f * (float) display->userArea.getWidth() / (float) width);
    setSize (juce::roundToInt ((float) width * scale), juce::roundToInt ((float) designHeight * scale));

    startTimerHz (10);
}

LP60Editor::~LP60Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void LP60Editor::beginSection (const juce::String& title, juce::Colour colour, int firstKeyColour)
{
    sections.push_back ({ title, (int) columns.size(), 0, colour });
    nextKeyColour = firstKeyColour;
}

void LP60Editor::addSlider (const char* paramId, const juce::String& title)
{
    auto slider = std::make_unique<juce::Slider> (juce::Slider::LinearVertical, juce::Slider::TextBoxBelow);
    styleTextBox (*slider);
    slider->setTextBoxStyle (juce::Slider::TextBoxBelow, true, columnWidth + 6, 14);
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, paramId, *slider));

    if (auto* parameter = processor.apvts.getParameter (paramId))
        slider->setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

    panel.addAndMakeVisible (*slider);
    columns.push_back ({ title, { slider.get() }, true });
    sliders.push_back (std::move (slider));
    ++sections.back().numColumns;
}

void LP60Editor::addButtons (std::initializer_list<std::pair<const char*, const char*>> list, const juce::String& title)
{
    Column column { title, {}, false };
    for (const auto& [id, name] : list)
    {
        auto button = std::make_unique<juce::ToggleButton> (name);
        button->getProperties().set ("key", (juce::int64) Colours::keys[nextKeyColour++ % 3].getARGB());
        if (juce::StringArray { "pulse", "saw", "subOn" }.contains (id))
            button->getProperties().set ("wave", juce::String (id));
        buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            processor.apvts, id, *button));
        panel.addAndMakeVisible (*button);
        column.components.push_back (button.get());
        buttons.push_back (std::move (button));
    }
    columns.push_back (column);
    ++sections.back().numColumns;
}

void LP60Editor::addChoice (const char* paramId, const juce::String& title, const juce::StringArray& labels)
{
    auto choice = std::make_unique<LedChoice> (*processor.apvts.getParameter (paramId), labels);
    panel.addAndMakeVisible (*choice);
    columns.push_back ({ title, { choice.get() }, false });
    choices.push_back (std::move (choice));
    ++sections.back().numColumns;
}

juce::Slider& LP60Editor::addKnob (const char* paramId)
{
    auto knob = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    styleTextBox (*knob);
    knob->setTextBoxStyle (juce::Slider::TextBoxBelow, true, knobWidth, 13);
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, paramId, *knob));
    if (auto* parameter = processor.apvts.getParameter (paramId))
        knob->setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    panel.addAndMakeVisible (*knob);
    sliders.push_back (std::move (knob));
    return *sliders.back();
}

juce::Rectangle<int> LP60Editor::columnArea (int column) const
{
    int sectionIndex = 0;
    for (size_t i = 0; i < sections.size(); ++i)
        if (column >= sections[i].firstColumn)
            sectionIndex = (int) i;

    return { left + column * columnWidth + sectionIndex * sectionGap, controlsTop, columnWidth, controlsHeight };
}

void LP60Editor::resized()
{
    for (size_t c = 0; c < columns.size(); ++c)
    {
        const auto& column = columns[c];
        auto area = columnArea ((int) c).withTrimmedTop (columnTitle);

        if (column.isSlider)
        {
            column.components.front()->setBounds (area.expanded (3, 0));
        }
        else if (dynamic_cast<LedChoice*> (column.components.front()) != nullptr)
        {
            column.components.front()->setBounds (area.withTrimmedTop (6).withHeight (100));
        }
        else
        {
            auto stack = area.withTrimmedTop (4);
            for (auto* component : column.components)
                component->setBounds (stack.removeFromTop (50));
        }
    }

    // Bas : presets dans l'en-tête, réglages de jeu, molettes, clavier.
    const int width = panel.getWidth();
    const int presetY = grilleHeight + (headerHeight - grilleHeight - bandHeight) / 2 + 6;
    presetBox.setBounds (width / 2 - 110, presetY, 220, 24);
    previousPreset.setBounds (presetBox.getX() - 30, presetY, 26, 24);
    nextPreset.setBounds (presetBox.getRight() + 4, presetY, 26, 24);

    auto bottom = juce::Rectangle<int> (left, bottomTop, width - 2 * left, bottomHeight);
    for (auto& [knob, title] : knobs)
        knob->setBounds (bottom.removeFromLeft (knobWidth).withTrimmedTop (16).withTrimmedBottom (14));

    bottom.removeFromLeft (8);
    auto wheels = bottom.removeFromLeft (wheelsWidth).reduced (6, 8);
    pitchWheel.setBounds (wheels.removeFromLeft (wheels.getWidth() / 2).reduced (3, 0));
    modWheel.setBounds (wheels.reduced (3, 0));

    if (playerPanel.isVisible())
        playerPanel.setBounds (bottom.removeFromLeft (playerWidth).reduced (4, 6));

    keyboard.setBounds (bottom);
    keyboard.setKeyWidth ((float) bottom.getWidth() / 36.0f);

    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) panel.getWidth()));
}

void LP60Editor::paintPanel (juce::Graphics& g)
{
    const auto bounds = panel.getLocalBounds();
    g.fillAll (Colours::panel);

    // Joues en bois.
    for (auto side : { bounds.withWidth (woodWidth), bounds.withLeft (bounds.getRight() - woodWidth) })
    {
        juce::ColourGradient wood (Colours::woodDark, (float) side.getX(), 0.0f,
                                   Colours::woodDark, (float) side.getRight(), 0.0f, false);
        wood.addColour (0.5, Colours::woodLight);
        g.setGradientFill (wood);
        g.fillRect (side);

        juce::Random grain (side.getX() + 11);
        g.setColour (Colours::woodDark.withAlpha (0.5f));
        for (int i = 0; i < 8; ++i)
        {
            const float gx = (float) side.getX() + 3.0f + grain.nextFloat() * (float) (woodWidth - 6);
            g.drawLine (gx, 0.0f, gx + grain.nextFloat() * 4.0f - 2.0f, (float) bounds.getHeight(), 0.8f);
        }
    }

    paintHeader (g, { woodWidth, 0, bounds.getWidth() - 2 * woodWidth, headerHeight });

    // Bandeaux de section pleins (blanc, bleu ou rouge), filet de la même
    // couleur sous les réglages, séparateurs fins.
    for (const auto& section : sections)
    {
        const auto first = columnArea (section.firstColumn);
        const auto last = columnArea (section.firstColumn + section.numColumns - 1);
        const auto bandArea = juce::Rectangle<int> (first.getX() - 3, headerHeight, last.getRight() - first.getX() + 6, bandHeight);
        const bool light = section.colour.getPerceivedBrightness() > 0.6f;

        g.setColour (section.colour);
        g.fillRect (bandArea.withTrimmedBottom (4));
        g.setColour (light ? juce::Colours::black : juce::Colours::white);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold).withKerningFactor (0.12f));
        g.drawText (section.title, bandArea.withTrimmedBottom (4), juce::Justification::centred);

        g.setColour (section.colour);
        g.fillRect (bandArea.withY (first.getBottom() - 1).withHeight (3));

        if (section.firstColumn > 0)
        {
            g.setColour (juce::Colour (0xff4a4a4a));
            const float sx = (float) first.getX() - sectionGap * 0.5f;
            g.drawLine (sx, (float) controlsTop + 4.0f, sx, (float) first.getBottom() - 6.0f, 1.0f);
        }
    }

    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    for (size_t c = 0; c < columns.size(); ++c)
    {
        if (columns[c].title.isEmpty())
            continue;
        g.setColour (Colours::text);
        g.drawText (columns[c].title, columnArea ((int) c).withHeight (columnTitle).expanded (6, 0),
                    juce::Justification::centred);
    }

    // Bas : bandeau des réglages de jeu.
    g.setColour (juce::Colour (0xff3a3a3a));
    g.drawHorizontalLine (bottomTop - 5, (float) left, (float) (bounds.getWidth() - left));
    g.setColour (Colours::text);
    auto knobTitles = juce::Rectangle<int> (left, bottomTop, knobWidth * (int) knobs.size(), 14);
    for (auto& entry : knobs)
        g.drawText (entry.second, knobTitles.removeFromLeft (knobWidth), juce::Justification::centred);
}

// En-tête façon Juno-60 : bandeau alu à grille d'aération, marque à gauche,
// nom du modèle en lettres chromées à droite.
void LP60Editor::paintHeader (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto grille = area.removeFromTop (grilleHeight).toFloat();
    g.setGradientFill (juce::ColourGradient (Colours::silver.brighter (0.2f), 0.0f, grille.getY(),
                                             Colours::silver.darker (0.25f), 0.0f, grille.getBottom(), false));
    g.fillRect (grille);

    g.setColour (juce::Colour (0xff2a2a2a));
    for (float x = grille.getX() + 20.0f; x < grille.getRight() - 300.0f; x += 5.0f)
        g.fillRect (juce::Rectangle<float> (2.0f, grille.getHeight() - 8.0f).withPosition (x, grille.getY() + 4.0f));

    g.setColour (juce::Colour (0xff1a1a1a));
    g.setFont (juce::FontOptions (8.5f, juce::Font::bold).withKerningFactor (0.15f));
    g.drawText ("PROGRAMMABLE POLYPHONIC SYNTHESIZER", grille.withTrimmedRight (14.0f).toNearestInt(),
                juce::Justification::centredRight);

    auto body = area.withTrimmedBottom (bandHeight - 14).reduced (12, 6);

    // Marque, dans un cadre.
    const auto brand = body.removeFromLeft (190).withSizeKeepingCentre (180, 30).toFloat();
    g.setColour (Colours::text);
    g.drawRoundedRectangle (brand, 3.0f, 1.6f);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold).withKerningFactor (0.2f));
    g.drawText ("LOAD & PLAY", brand.toNearestInt(), juce::Justification::centred);

    // Nom du modèle : lettres larges, dégradé chromé, contour noir, rainures.
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (juce::Font (juce::FontOptions (52.0f, juce::Font::bold).withHorizontalScale (1.3f)), "LP-60", 0.0f, 0.0f);
    juce::Path title;
    glyphs.createPath (title);
    const auto target = body.removeFromRight (300).toFloat().reduced (0.0f, 2.0f);
    title.applyTransform (juce::RectanglePlacement (juce::RectanglePlacement::xRight | juce::RectanglePlacement::yMid)
                              .getTransformToFit (title.getBounds(), target));

    juce::ColourGradient chrome (juce::Colour (0xfff6f6f3), 0.0f, target.getY(), juce::Colour (0xff7d7d79), 0.0f, target.getBottom(), false);
    chrome.addColour (0.48, juce::Colour (0xffd0d0cb));
    chrome.addColour (0.52, juce::Colour (0xff9c9c97));
    g.setGradientFill (chrome);
    g.fillPath (title);

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (title);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (float y = target.getY(); y < target.getBottom(); y += 3.0f)
            g.drawHorizontalLine ((int) y, target.getX(), target.getRight());
    }

    g.setColour (juce::Colours::black);
    g.strokePath (title, juce::PathStrokeType (1.5f));
}

void LP60Editor::refreshPresetBox()
{
    shownProgram = processor.getCurrentProgram();
    presetBox.setSelectedItemIndex (shownProgram, juce::dontSendNotification);
}

void LP60Editor::timerCallback()
{
    if (processor.getCurrentProgram() != shownProgram)
        refreshPresetBox();
}

//==============================================================================
bool LP60Editor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi") || f.endsWithIgnoreCase (".smf"))
            return true;
    return false;
}

void LP60Editor::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    repaint();

    for (const auto& f : files)
    {
        if (! isInterestedInFileDrag ({ f }))
            continue;

        if (processor.midiPlayer.load (juce::File (f)))
        {
            playerPanel.refresh();
            playerPanel.setVisible (true);
            resized();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "LP-60",
                juce::String::fromUTF8 ("Ce fichier ne contient pas de notes MIDI lisibles."));
        }
        return;
    }
}

void LP60Editor::paintOverChildren (juce::Graphics& g)
{
    if (! dragOver)
        return;

    // Pendant le glisser : la fenêtre s'encadre et invite à déposer.
    const auto area = getLocalBounds().toFloat().reduced (3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (area);
    g.setColour (Colours::accent);
    g.drawRoundedRectangle (area, 8.0f, 3.0f);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("D\xc3\xa9posez le fichier MIDI pour le jouer"), area.toNearestInt(),
                juce::Justification::centred);
}

void LP60Editor::sendWheel (bool pitch, double value)
{
    const auto message = pitch ? juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, juce::roundToInt ((value + 1.0) * 8191.5)))
                               : juce::MidiMessage::controllerEvent (1, 1, juce::roundToInt (value * 127.0));
    processor.uiMidi.addMessageToQueue (message.withTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001));
}
