#include "PluginEditor.h"

namespace
{
    namespace Colours
    {
        const juce::Colour panel     { 0xff161616 };
        const juce::Colour text      { 0xfff2efe6 };
        const juce::Colour dimText   { 0xffa8a39a };
        const juce::Colour separator { 0xff3c3c3c };
        const juce::Colour woodDark  { 0xff4a2a14 };
        const juce::Colour woodLight { 0xff7b4a26 };
        const juce::Colour rockerOn  { 0xffe8762c };
        const juce::Colour rockerOff { 0xff3a3a3a };
        const juce::Colour ledOn     { 0xffff3b2f };
    }

    constexpr int woodWidth = 22;
    constexpr int innerMargin = 12;
    constexpr int cellWidth = 66;
    constexpr int cellHeight = 96;
    constexpr int sectionGap = 16;
    constexpr int headerHeight = 44;
    constexpr int titleHeight = 22;
    constexpr int bandGap = 10;
    constexpr int labelHeight = 13;
    constexpr int keyboardHeight = 84;
    constexpr int wheelsWidth = 74;
    constexpr int playerWidth = 190;
    constexpr int numColumns = 19;
    constexpr int numSections = 5;
    constexpr int numBands = 3;
    constexpr int rowsInBand[numBands] = { 3, 1, 1 };

    constexpr int left = woodWidth + innerMargin;
    constexpr int designWidth = 2 * left + numColumns * cellWidth + (numSections - 1) * sectionGap;

    constexpr int bandTopFor (int band)
    {
        int y = headerHeight + titleHeight;
        for (int b = 0; b < band; ++b)
            y += rowsInBand[b] * cellHeight + bandGap + titleHeight;
        return y;
    }

    constexpr int keyboardTop = bandTopFor (numBands - 1) + cellHeight + 8;
    constexpr int designHeight = keyboardTop + keyboardHeight;
}

//==============================================================================
PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::dimText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::TextButton::buttonColourId, Colours::rockerOff);
    setColour (juce::TextButton::buttonOnColourId, Colours::rockerOn);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 7.0f;

    // Graduations : 0 à 10 pour un potard continu, une par position pour un sélecteur.
    const auto range = slider.getRange();
    const int numTicks = slider.getInterval() >= 1.0 ? juce::jmin (13, juce::roundToInt (range.getLength()) + 1) : 11;

    g.setColour (Colours::text);
    for (int i = 0; i < numTicks; ++i)
    {
        const float angle = startAngle + (endAngle - startAngle) * (float) i / (float) (numTicks - 1);
        const auto inner = centre.getPointOnCircumference (radius + 2.0f, angle);
        const auto outer = centre.getPointOnCircumference (radius + 5.5f, angle);
        g.drawLine ({ inner, outer }, 1.1f);
    }

    // Jupe noire crantée.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), centre.translated (-radius * 0.5f, -radius * 0.5f),
                                             juce::Colour (0xff050505), centre.translated (radius, radius), true));
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));

    g.setColour (juce::Colour (0xff000000));
    for (int i = 0; i < 36; ++i)
    {
        const float angle = juce::MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine ({ centre.getPointOnCircumference (radius * 0.82f, angle),
                      centre.getPointOnCircumference (radius, angle) }, 1.0f);
    }

    // Chapeau alu.
    const float capRadius = radius * 0.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e6e6), centre.translated (-capRadius, -capRadius),
                                             juce::Colour (0xff7a7a7a), centre.translated (capRadius, capRadius), false));
    g.fillEllipse (juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre));

    // Index blanc.
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    g.setColour (Colours::text);
    g.drawLine ({ centre.getPointOnCircumference (capRadius * 1.05f, angle),
                  centre.getPointOnCircumference (radius * 0.95f, angle) }, 2.2f);
}

void PanelLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    // Molette : fente sombre, roue grise striée, repère blanc.
    const auto slot = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f, 0.0f);
    g.setColour (juce::Colour (0xff0b0b0b));
    g.fillRoundedRectangle (slot, 4.0f);

    const auto wheel = slot.reduced (3.0f, 4.0f);
    juce::ColourGradient shading (juce::Colour (0xff1e1e1e), 0.0f, wheel.getY(), juce::Colour (0xff1e1e1e), 0.0f, wheel.getBottom(), false);
    shading.addColour (0.5, juce::Colour (0xff6a6a6a));
    g.setGradientFill (shading);
    g.fillRoundedRectangle (wheel, 3.0f);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (float ry = wheel.getY() + 3.0f; ry < wheel.getBottom(); ry += 4.0f)
        g.drawHorizontalLine ((int) ry, wheel.getX() + 1.0f, wheel.getRight() - 1.0f);

    g.setColour (Colours::text);
    g.fillRect (juce::Rectangle<float> (wheel.getWidth(), 3.0f).withCentre ({ wheel.getCentreX(), sliderPos }));
}

void PanelLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    auto textArea = bounds.removeFromBottom ((float) labelHeight);

    const float rockerHeight = juce::jmin (17.0f, bounds.getHeight() - 4.0f);
    const auto rocker = juce::Rectangle<float> (32.0f, rockerHeight).withCentre (bounds.getCentre());
    const bool on = button.getToggleState();

    g.setColour ((on ? Colours::rockerOn : Colours::rockerOff).brighter (highlighted ? 0.15f : 0.0f));
    g.fillRoundedRectangle (rocker, 3.0f);

    // Moitié enfoncée de la bascule.
    auto pressed = rocker.reduced (2.0f);
    pressed = on ? pressed.removeFromTop (pressed.getHeight() * 0.5f) : pressed.removeFromBottom (pressed.getHeight() * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (pressed, 2.0f);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawFittedText (button.getButtonText(), textArea.toNearestInt(), juce::Justification::centred, 1);
}

//==============================================================================
void StepButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto bounds = getLocalBounds().toFloat();
    const auto textArea = bounds.removeFromBottom ((float) labelHeight);
    const auto led = juce::Rectangle<float> (14.0f, 14.0f).withCentre (bounds.getCentre());

    if (getToggleState())
    {
        g.setColour (Colours::ledOn.withAlpha (0.25f));
        g.fillEllipse (led.expanded (4.0f));
        g.setColour (Colours::ledOn.brighter (highlighted ? 0.2f : 0.0f));
    }
    else
    {
        g.setColour (juce::Colour (0xff4a1612).brighter (highlighted ? 0.3f : 0.0f));
    }
    g.fillEllipse (led);

    g.setColour (Colours::dimText);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText (getButtonText(), textArea, juce::Justification::centred);
}

//==============================================================================
ModelDEditor::ModelDEditor (ModelDProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      playerPanel (p.midiPlayer, Colours::rockerOn, Colours::text)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (panel);
    panel.setSize (designWidth, designHeight);
    panel.addMouseListener (this, true);

    sections = { { "CONTROLLERS", 0, 0, 3 },
                 { "OSCILLATOR BANK", 0, 3, 6 },
                 { "MIXER", 0, 9, 3 },
                 { "MODIFIERS", 0, 12, 6 },
                 { "OUTPUT", 0, 18, 1 },
                 { "LFO", 1, 0, 6 },
                 { "OVERDRIVE", 1, 6, 4 },
                 { "CHORUS", 1, 10, 2 },
                 { "DELAY", 1, 12, 6 },
                 { "MIDI CC", 1, 18, 1 },
                 { "ARPEGGIATOR", 2, 0, 19 } };

    //--------------------------------------------------------------------------
    // Rangée principale : la façade façon Minimoog, plus les options.

    // Controllers
    addKnob ("tune", "TUNE", 0, 0, 0);
    addKnob ("glide", "GLIDE", 0, 0, 1);
    addKnob ("modMix", "MOD MIX", 0, 0, 2);
    addKnob ("modAmount", "MOD AMOUNT", 0, 1, 0);
    addKnob ("bendRange", "BEND RANGE", 0, 1, 1);
    addKnob ("drift", "DRIFT", 0, 1, 2);
    addSwitch ("oscMod", "OSC MOD", 0, 2, 0, top);
    addSwitch ("modFromLfo", "MOD: LFO", 0, 2, 0, bottom);
    addSwitch ("osc3Ctrl", "OSC 3 CTRL", 0, 2, 1);
    addSwitch ("retrigger", "RETRIG", 0, 2, 2, top);
    addSwitch ("poly", "POLY", 0, 2, 2, bottom);

    // Oscillator bank : range, frequency, fine, waveform, offset, rectify
    for (int i = 0; i < 3; ++i)
    {
        const auto n = juce::String (i + 1);
        addKnob (("osc" + n + "Range").toRawUTF8(), "RANGE", 0, 3, i);
        addKnob (("osc" + n + "Freq").toRawUTF8(), "FREQUENCY", 0, 4, i);
        if (i > 0)
            addKnob (("osc" + n + "Fine").toRawUTF8(), "FINE TUNE", 0, 5, i);
        addKnob (("osc" + n + "Wave").toRawUTF8(), "WAVEFORM", 0, 6, i);
        addKnob (("osc" + n + "Offset").toRawUTF8(), "OFFSET", 0, 7, i);
        addSwitch (("osc" + n + "Rect").toRawUTF8(), "RECTIFY", 0, 8, i);
    }
    addSwitch ("oscSync", "SYNC 2>1", 0, 5, 0);

    // Mixer
    addKnob ("osc1Level", "OSC 1", 0, 9, 0);
    addKnob ("osc2Level", "OSC 2", 0, 9, 1);
    addKnob ("osc3Level", "OSC 3", 0, 9, 2);
    addSwitch ("osc1On", "ON", 0, 10, 0);
    addSwitch ("osc2On", "ON", 0, 10, 1);
    addSwitch ("osc3On", "ON", 0, 10, 2);
    addKnob ("noiseLevel", "NOISE", 0, 11, 0);
    addSwitch ("noiseOn", "ON", 0, 11, 1);
    addSwitch ("pinkNoise", "PINK", 0, 11, 2);

    // Modifiers : filtre en haut, contours en dessous
    addKnob ("cutoff", "CUTOFF", 0, 12, 0);
    addKnob ("emphasis", "EMPHASIS", 0, 13, 0);
    addKnob ("contour", "CONTOUR AMT", 0, 14, 0);
    addKnob ("velocity", "VELOCITY", 0, 15, 0);
    addKnob ("vco3Filter", "VCO 3 FILT", 0, 16, 0);
    addSwitch ("filterMod", "FILTER MOD", 0, 17, 0, top);
    addSwitch ("bassComp", "BASS COMP", 0, 17, 0, bottom);
    addKnob ("fAttack", "F. ATTACK", 0, 12, 1);
    addKnob ("fDecay", "F. DECAY", 0, 13, 1);
    addKnob ("fSustain", "F. SUSTAIN", 0, 14, 1);
    addKnob ("fRelease", "F. RELEASE", 0, 15, 1);
    addKnob ("aftertouch", "AFTERTOUCH", 0, 16, 1);
    addSwitch ("kbd1", "KBD 1", 0, 17, 1, top);
    addSwitch ("kbd2", "KBD 2", 0, 17, 1, bottom);
    addKnob ("lAttack", "L. ATTACK", 0, 12, 2);
    addKnob ("lDecay", "L. DECAY", 0, 13, 2);
    addKnob ("lSustain", "L. SUSTAIN", 0, 14, 2);
    addKnob ("lRelease", "L. RELEASE", 0, 15, 2);
    addSwitch ("decay", "DECAY", 0, 17, 2, top);
    addSwitch ("adsr", "ADSR", 0, 17, 2, bottom);

    // Output
    addKnob ("volume", "VOLUME", 0, 18, 0);
    addKnob ("bpm", "TEMPO", 0, 18, 1);

    //--------------------------------------------------------------------------
    // Rangée des effets
    addKnob ("lfoLevel", "LEVEL", 1, 0, 0);
    addKnob ("lfoWave", "WAVE", 1, 1, 0);
    addKnob ("lfoRate", "RATE", 1, 2, 0);
    addKnob ("lfoDivision", "SYNC RATE", 1, 3, 0);
    addKnob ("lfoDest", "DESTINATION", 1, 4, 0);
    addSwitch ("lfoSync", "TEMPO SYNC", 1, 5, 0, top);
    addSwitch ("autopan", "AUTOPAN", 1, 5, 0, bottom);

    addSwitch ("driveOn", "ON", 1, 6, 0);
    addKnob ("driveGain", "GAIN", 1, 7, 0);
    addKnob ("driveLowCut", "LOW CUT", 1, 8, 0);
    addKnob ("driveHighCut", "HIGH CUT", 1, 9, 0);

    addKnob ("chorusMode", "MODE", 1, 10, 0);
    addKnob ("chorusLevel", "LEVEL", 1, 11, 0);

    addSwitch ("delayOn", "ON", 1, 12, 0, top);
    addSwitch ("delaySync", "SYNC", 1, 12, 0, bottom);
    addKnob ("delayLevel", "LEVEL", 1, 13, 0);
    addKnob ("delayTime", "TIME", 1, 14, 0);
    addKnob ("delayDivision", "SYNC TIME", 1, 15, 0);
    addKnob ("delayFeedback", "FEEDBACK", 1, 16, 0);
    addSwitch ("delayRight2x", "RIGHT 2X", 1, 17, 0);

    learnButton.setClickingTogglesState (true);
    learnButton.onClick = [this]
    {
        if (! learnButton.getToggleState())
            processor.cancelLearn();
        timerCallback();
    };
    resetButton.onClick = [this] { processor.clearMidiMap(); panel.repaint(); };
    for (auto* b : { &learnButton, &resetButton })
        panel.addAndMakeVisible (b);
    placements.push_back ({ &learnButton, 1, 18, 0, top });
    placements.push_back ({ &resetButton, 1, 18, 0, bottom });

    //--------------------------------------------------------------------------
    // Arpégiateur
    addSwitch ("arpOn", "ON", 2, 0, 0);
    addKnob ("arpMode", "MODE", 2, 1, 0);
    addKnob ("arpDivision", "SPEED", 2, 2, 0);
    addKnob ("arpRange", "RANGE", 2, 3, 0);
    addSwitch ("arpGate", "GATE", 2, 4, 0);

    for (int i = 1; i <= 16; ++i)
    {
        auto sw = std::make_unique<Switch>();
        sw->button = std::make_unique<StepButton> (juce::String (i));
        const auto id = "arpStep" + juce::String (i);
        sw->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            processor.apvts, id, *sw->button);
        panel.addAndMakeVisible (*sw->button);
        controls.push_back ({ id, sw->button.get() });
        switches.push_back (std::move (sw));
    }

    //--------------------------------------------------------------------------
    // Molettes et clavier
    pitchWheel.setRange (-1.0, 1.0);
    pitchWheel.setValue (0.0, juce::dontSendNotification);
    pitchWheel.onValueChange = [this] { sendWheel (true, pitchWheel.getValue()); };
    pitchWheel.onDragEnd = [this] { pitchWheel.setValue (0.0); };   // revient au centre

    modWheel.setRange (0.0, 1.0);
    modWheel.onValueChange = [this] { sendWheel (false, modWheel.getValue()); };

    for (auto* wheel : { &pitchWheel, &modWheel })
        panel.addAndMakeVisible (wheel);

    keyboard.setAvailableRange (24, 108);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Colours::rockerOn.withAlpha (0.7f));
    panel.addAndMakeVisible (keyboard);

    // Lecteur MIDI : invisible tant qu'aucun fichier n'est glissé sur la fenêtre.
    playerPanel.onClose = [this] { playerPanel.setVisible (false); resized(); };
    playerPanel.setVisible (processor.midiPlayer.isLoaded());
    playerPanel.refresh();
    panel.addChildComponent (playerPanel);

    // Fenêtre redimensionnable, proportions gardées ; plus petite si l'écran l'exige.
    setResizable (true, true);
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);

    float scale = 1.0f;
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        scale = juce::jmin (1.0f, 0.92f * (float) display->userArea.getWidth() / (float) designWidth,
                            0.88f * (float) display->userArea.getHeight() / (float) designHeight);
    setSize (juce::roundToInt ((float) designWidth * scale), juce::roundToInt ((float) designHeight * scale));

    startTimerHz (10);
}

ModelDEditor::~ModelDEditor()
{
    stopTimer();
    processor.cancelLearn();
    setLookAndFeel (nullptr);
}

void ModelDEditor::addKnob (const char* paramId, const juce::String& text, int band, int column, int row)
{
    auto knob = std::make_unique<Knob>();

    knob->slider.setColour (juce::Slider::textBoxTextColourId, Colours::dimText);
    knob->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, cellWidth - 2, 15);
    knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, paramId, knob->slider);

    // Double-clic : retour à la valeur par défaut.
    if (auto* parameter = processor.apvts.getParameter (paramId))
        knob->slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

    knob->label.setText (text, juce::dontSendNotification);
    knob->label.setJustificationType (juce::Justification::centred);
    knob->label.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    knob->label.setInterceptsMouseClicks (false, false);

    panel.addAndMakeVisible (knob->slider);
    panel.addAndMakeVisible (knob->label);

    placements.push_back ({ &knob->label, band, column, row, knobLabel });
    placements.push_back ({ &knob->slider, band, column, row, knobBody });
    controls.push_back ({ paramId, &knob->slider });
    knobs.push_back (std::move (knob));
}

void ModelDEditor::addSwitch (const char* paramId, const juce::String& text, int band, int column, int row, int kind)
{
    auto sw = std::make_unique<Switch>();

    sw->button = std::make_unique<juce::ToggleButton> (text);
    sw->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, paramId, *sw->button);

    panel.addAndMakeVisible (*sw->button);

    placements.push_back ({ sw->button.get(), band, column, row, kind });
    controls.push_back ({ paramId, sw->button.get() });
    switches.push_back (std::move (sw));
}

int ModelDEditor::bandTop (int band) const
{
    return bandTopFor (band);
}

juce::Rectangle<int> ModelDEditor::cell (int band, int column, int row) const
{
    int sectionIndex = 0;
    for (const auto& section : sections)
        if (section.band == band && column >= section.firstColumn)
            sectionIndex = (int) (&section - sections.data());

    int indexInBand = 0;
    for (int i = 0; i < sectionIndex; ++i)
        if (sections[(size_t) i].band == band)
            ++indexInBand;

    const int x = left + column * cellWidth + indexInBand * sectionGap;
    const int y = bandTop (band) + row * cellHeight;
    return { x, y, cellWidth, cellHeight };
}

juce::Rectangle<int> ModelDEditor::stepArea() const
{
    const auto first = cell (2, 5, 0);
    return { first.getX() + 20, first.getY(), designWidth - left - first.getX() - 20, cellHeight };
}

void ModelDEditor::resized()
{
    for (const auto& p : placements)
    {
        auto area = cell (p.band, p.column, p.row).reduced (2, 4);

        if (p.kind == knobLabel)
            area = area.removeFromTop (labelHeight);
        else if (p.kind == knobBody)
            area = area.withTrimmedTop (labelHeight);
        else if (p.kind == top)
            area = area.removeFromTop (area.getHeight() / 2).reduced (0, 1);
        else if (p.kind == bottom)
            area = area.withTrimmedTop (area.getHeight() / 2).reduced (0, 1);

        // Les boutons LEARN / RESET sont plus petits que les bascules.
        if (p.component == &learnButton || p.component == &resetButton)
            area = area.withSizeKeepingCentre (area.getWidth() - 6, 22);

        p.component->setBounds (area);
    }

    // Motif d'arpège : 16 LED réparties sur la largeur.
    const auto steps = stepArea().withTrimmedTop (labelHeight + 8);
    const int stepWidth = steps.getWidth() / 16;
    int stepIndex = 0;
    for (const auto& control : controls)
        if (control.paramId.startsWith ("arpStep"))
            control.component->setBounds (steps.getX() + stepIndex++ * stepWidth, steps.getY(), stepWidth,
                                          steps.getHeight() - 8);

    // Molettes puis clavier.
    auto bottomArea = juce::Rectangle<int> (woodWidth, keyboardTop, designWidth - 2 * woodWidth, keyboardHeight);
    auto wheels = bottomArea.removeFromLeft (wheelsWidth).reduced (8, 8);
    pitchWheel.setBounds (wheels.removeFromLeft (wheels.getWidth() / 2).reduced (3, 0));
    modWheel.setBounds (wheels.reduced (3, 0));
    if (playerPanel.isVisible())
        playerPanel.setBounds (bottomArea.removeFromLeft (playerWidth).reduced (4, 6));
    keyboard.setBounds (bottomArea);
    keyboard.setKeyWidth ((float) bottomArea.getWidth() / 50.0f);

    const float scale = (float) getWidth() / (float) designWidth;
    panel.setTransform (juce::AffineTransform::scale (scale));
}

void ModelDEditor::paintPanel (juce::Graphics& g)
{
    const auto bounds = panel.getLocalBounds();

    g.fillAll (Colours::panel);

    // Joues en bois avec un peu de veinage, et bandeau sous le panneau.
    auto paintWood = [&g, &bounds] (juce::Rectangle<int> area, bool vertical)
    {
        juce::ColourGradient wood (Colours::woodDark, (float) area.getX(), (float) area.getY(),
                                   Colours::woodDark, vertical ? (float) area.getRight() : (float) area.getX(),
                                   vertical ? (float) area.getY() : (float) area.getBottom(), false);
        wood.addColour (0.5, Colours::woodLight);
        g.setGradientFill (wood);
        g.fillRect (area);

        juce::Random grain (area.getX() + area.getY() + 7);
        g.setColour (Colours::woodDark.withAlpha (0.5f));
        for (int i = 0; i < 9; ++i)
        {
            if (vertical)
            {
                const float gx = (float) area.getX() + 3.0f + grain.nextFloat() * (float) (area.getWidth() - 6);
                g.drawLine (gx, 0.0f, gx + grain.nextFloat() * 4.0f - 2.0f, (float) bounds.getHeight(), 0.8f);
            }
            else
            {
                const float gy = (float) area.getY() + 2.0f + grain.nextFloat() * (float) (area.getHeight() - 4);
                g.drawLine ((float) area.getX(), gy, (float) area.getRight(), gy + grain.nextFloat() * 3.0f - 1.5f, 0.8f);
            }
        }
    };

    paintWood (bounds.withWidth (woodWidth), true);
    paintWood (bounds.withLeft (bounds.getRight() - woodWidth), true);
    paintWood ({ woodWidth, keyboardTop, wheelsWidth, keyboardHeight }, false);

    // En-tête.
    const auto header = juce::Rectangle<int> (left, 0, designWidth - 2 * left, headerHeight);
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold | juce::Font::italic));
    g.drawText ("Model Play", header, juce::Justification::centredLeft);

    // Marque à droite, espacée comme un logo de façade.
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold).withKerningFactor (0.25f));
    g.drawText ("LOAD & PLAY", header, juce::Justification::centredRight);

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (status.isNotEmpty() ? Colours::rockerOn : Colours::dimText);
    g.drawText (status.isNotEmpty() ? status : juce::String ("ANALOG-STYLE SYNTHESIZER"),
                header.withTrimmedLeft (150).withTrimmedRight (190), juce::Justification::centred);

    g.setColour (Colours::separator);
    g.drawHorizontalLine (headerHeight - 4, (float) header.getX(), (float) header.getRight());

    // Titres de section et séparateurs.
    for (const auto& section : sections)
    {
        const auto first = cell (section.band, section.firstColumn, 0);
        const auto lastColumn = section.band == 2 ? 4 : section.firstColumn + section.numColumns - 1;
        const auto last = cell (section.band, lastColumn, rowsInBand[section.band] - 1);
        const auto titleArea = juce::Rectangle<int> (first.getX(), first.getY() - titleHeight,
                                                     (section.band == 2 ? header.getRight() : last.getRight()) - first.getX(),
                                                     titleHeight);

        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (section.title, titleArea, section.band == 2 ? juce::Justification::centredLeft
                                                                : juce::Justification::centred);

        if (section.firstColumn > 0)
        {
            g.setColour (Colours::separator);
            const float sx = (float) first.getX() - (float) sectionGap * 0.5f;
            g.drawLine (sx, (float) titleArea.getY() + 4.0f, sx, (float) last.getBottom(), 1.0f);
        }

        if (section.band > 0 && section.firstColumn == 0)
        {
            g.setColour (Colours::separator);
            g.drawHorizontalLine (titleArea.getY() - bandGap / 2, (float) header.getX(), (float) header.getRight());
        }
    }

    // Repères de rangée de l'oscillator bank et légende du motif d'arpège.
    g.setColour (Colours::dimText);
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    for (int i = 0; i < 3; ++i)
    {
        const auto c = cell (0, 3, i);
        g.drawText (juce::String (i + 1), c.getX() - 9, c.getY() + 2, 10, 12, juce::Justification::centred);
    }

    const auto steps = stepArea();
    g.drawText ("GATE PATTERN (16 STEPS)", steps.withHeight (labelHeight + 6), juce::Justification::centredLeft);
}

void ModelDEditor::paintOverlay (juce::Graphics& g)
{
    if (! learnButton.getToggleState())
        return;

    // En mode MIDI learn : numéro de CC sur les réglages associés, cadre sur la cible.
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    for (const auto& control : controls)
    {
        const auto area = control.component->getBounds();

        if (const int cc = processor.getCcFor (control.paramId); cc >= 0)
        {
            const auto tag = juce::Rectangle<int> (area.getX(), area.getY(), 30, 12);
            g.setColour (Colours::rockerOn);
            g.fillRoundedRectangle (tag.toFloat(), 2.0f);
            g.setColour (juce::Colours::black);
            g.drawText ("CC" + juce::String (cc), tag, juce::Justification::centred);
        }

        if (control.paramId == learnTarget)
        {
            g.setColour (Colours::rockerOn);
            g.drawRoundedRectangle (area.toFloat().expanded (2.0f), 4.0f, 2.0f);
        }
    }
}

void ModelDEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! learnButton.getToggleState())
        return;

    for (const auto& control : controls)
    {
        if (control.component == e.eventComponent || control.component->isParentOf (e.eventComponent))
        {
            processor.setLearnTarget (control.paramId);
            timerCallback();
            return;
        }
    }
}

void ModelDEditor::timerCallback()
{
    const auto target = processor.getLearnTarget();
    juce::String newStatus;

    if (learnButton.getToggleState())
    {
        if (target.isNotEmpty())
        {
            const auto* parameter = processor.apvts.getParameter (target);
            newStatus = juce::String::fromUTF8 ("Bougez un potard de votre contr\xc3\xb4leur pour : ")
                      + (parameter != nullptr ? parameter->getName (64) : target);
        }
        else
        {
            const auto last = processor.getLastLearned();
            newStatus = (last.isNotEmpty() ? last + "   |   " : juce::String())
                      + juce::String::fromUTF8 ("MIDI LEARN : cliquez sur un r\xc3\xa9glage");
        }
    }

    if (target != learnTarget || newStatus != status)
    {
        learnTarget = target;
        status = newStatus;
        panel.repaint();
    }
}

void ModelDEditor::sendWheel (bool pitch, double value)
{
    const auto message = pitch ? juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, juce::roundToInt ((value + 1.0) * 8191.5)))
                               : juce::MidiMessage::controllerEvent (1, 1, juce::roundToInt (value * 127.0));
    processor.uiMidi.addMessageToQueue (message.withTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001));
}

//==============================================================================
bool ModelDEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi") || f.endsWithIgnoreCase (".smf"))
            return true;
    return false;
}

void ModelDEditor::filesDropped (const juce::StringArray& files, int, int)
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
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Model Play",
                juce::String::fromUTF8 ("Ce fichier ne contient pas de notes MIDI lisibles."));
        }
        return;
    }
}

void ModelDEditor::paintOverChildren (juce::Graphics& g)
{
    if (! dragOver)
        return;

    // Pendant le glisser : la fenêtre s'encadre et invite à déposer.
    const auto area = getLocalBounds().toFloat().reduced (3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (area);
    g.setColour (Colours::rockerOn);
    g.drawRoundedRectangle (area, 8.0f, 3.0f);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("D\xc3\xa9posez le fichier MIDI pour le jouer"), area.toNearestInt(),
                juce::Justification::centred);
}
