#include "PluginEditor.h"

using namespace chopchop;

//==============================================================================
PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::dim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Colours::accent.withAlpha (0.4f));
    setColour (juce::TextEditor::textColourId, Colours::text);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::TextButton::textColourOffId, Colours::dim);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff111111));
    setColour (juce::ComboBox::textColourId, Colours::accent);
    setColour (juce::ComboBox::arrowColourId, Colours::dim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff202020));
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff111111));
}

void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 7.0f;

    // Graduations : 11 pour un potard continu, une par position pour un sélecteur.
    const auto range = slider.getRange();
    const int numTicks = slider.getInterval() >= 1.0 && range.getLength() <= 12.0 ? juce::roundToInt (range.getLength()) + 1 : 11;
    g.setColour (Colours::dim);
    for (int i = 0; i < numTicks; ++i)
    {
        const float angle = startAngle + (endAngle - startAngle) * (float) i / (float) juce::jmax (1, numTicks - 1);
        g.drawLine ({ centre.getPointOnCircumference (radius + 2.0f, angle), centre.getPointOnCircumference (radius + 5.5f, angle) }, 1.1f);
    }

    // Jupe noire striée.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), centre.translated (-radius * 0.5f, -radius * 0.5f),
                                             juce::Colour (0xff050505), centre.translated (radius, radius), true));
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
    g.setColour (juce::Colours::black);
    for (int i = 0; i < 36; ++i)
    {
        const float angle = juce::MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine ({ centre.getPointOnCircumference (radius * 0.82f, angle), centre.getPointOnCircumference (radius, angle) }, 1.0f);
    }

    // Chapeau alu et index blanc.
    const float cap = radius * 0.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f0f0), centre.translated (-cap, -cap),
                                             juce::Colour (0xff767676), centre.translated (cap, cap), false));
    g.fillEllipse (juce::Rectangle<float> (cap * 2.0f, cap * 2.0f).withCentre (centre));

    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    g.setColour (slider.isEnabled() ? Colours::text : Colours::dim);
    g.drawLine ({ centre.getPointOnCircumference (cap * 1.05f, angle), centre.getPointOnCircumference (radius * 0.95f, angle) }, 2.2f);
}

void PanelLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const auto pill = juce::Rectangle<float> (36.0f, 20.0f).withCentre ({ bounds.getCentreX(), bounds.getY() + 46.0f });
    const bool on = button.getToggleState();

    g.setColour (on ? Colours::accent : juce::Colour (0xff3a3a3a));
    g.fillRoundedRectangle (pill, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (pill.withTrimmedTop (pill.getHeight() * 0.62f), 3.0f);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (pill, 3.0f);
    }

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (button.getButtonText(), bounds.withTop (pill.getBottom() + 8.0f).withHeight (16.0f), juce::Justification::centredTop);
}

void PanelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool)
{
    const auto r = button.getLocalBounds().toFloat().reduced (1.5f);
    g.setColour (button.getToggleState() ? Colours::accent : juce::Colour (highlighted ? 0xff353535 : 0xff2a2a2a));
    g.fillRoundedRectangle (r, 4.0f);
}

juce::Font PanelLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return juce::FontOptions (12.0f, juce::Font::bold); }
juce::Font PanelLookAndFeel::getComboBoxFont (juce::ComboBox&) { return juce::FontOptions (14.0f, juce::Font::bold); }

void PanelLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
    g.setColour (juce::Colour (0xff070707));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0xff4a4a4a));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    juce::Path arrow;
    const float cx = (float) width - 14.0f, cy = (float) height * 0.5f;
    arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour (Colours::dim);
    g.fillPath (arrow);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& n)
    : name (n), attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, Colours::dim);
    slider.setDoubleClickReturnValue (true, state.getParameter (id)->convertFrom0to1 (state.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (slider);
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (isEnabled() ? Colours::text : Colours::dim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (name, getLocalBounds().removeFromTop (16), juce::Justification::centred);
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (16));
}

Switch::Switch (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& n)
    : attachment (state, id, button)
{
    button.setButtonText (n);
    addAndMakeVisible (button);
}

Segmented::Segmented (juce::RangedAudioParameter& param, const juce::StringArray& labels)
    : attachment (param, [this] (float v)
                  {
                      current = juce::roundToInt (v);
                      for (int i = 0; i < buttons.size(); ++i)
                          buttons[i]->setToggleState (i == current, juce::dontSendNotification);
                  })
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    attachment.sendInitialUpdate();
}

void Segmented::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / juce::jmax (1, buttons.size());
    for (auto* b : buttons)
        b->setBounds (r.removeFromLeft (w));
}


juce::Colour Colours::forStep (int type)
{
    switch ((Step) type)
    {
        case Step::repeat:  return accent;
        case Step::roll:    return juce::Colour (0xffff6a5c);
        case Step::reverse: return juce::Colour (0xffb48cff);
        case Step::up:      return juce::Colour (0xfff2d34f);
        case Step::down:    return juce::Colour (0xff6fb7ff);
        case Step::mute:    return juce::Colour (0xff4a4a4a);
        case Step::thru:    break;
    }
    return juce::Colour (0xff262626);
}

//==============================================================================
StepSequencer::StepSequencer (ChopChopProcessor& p) : processor (p)
{
    for (int i = 0; i < numSteps; ++i)
        attachments.push_back (std::make_unique<juce::ParameterAttachment> (
            *p.state.getParameter (ChopChopProcessor::stepId (i)),
            [this, i] (float v) { types[(size_t) i] = juce::roundToInt (v); repaint(); }));
    for (auto& a : attachments)
        a->sendInitialUpdate();
    startTimerHz (30);
}

juce::Rectangle<float> StepSequencer::cellBounds (int index) const
{
    const float w = (float) getWidth() / numSteps;
    return { w * (float) index + 2.0f, 0.0f, w - 4.0f, (float) getHeight() };
}

int StepSequencer::cellAt (juce::Point<float> point) const
{
    return juce::jlimit (0, numSteps - 1, (int) (point.x / ((float) getWidth() / numSteps)));
}

void StepSequencer::setStep (int index, int type)
{
    attachments[(size_t) index]->setValueAsCompleteGesture ((float) type);
}

void StepSequencer::paint (juce::Graphics& g)
{
    for (int i = 0; i < numSteps; ++i)
    {
        const auto r = cellBounds (i);
        const int type = types[(size_t) i];
        const auto colour = Colours::forStep (type);
        g.setColour (i % 4 == 0 && type == 0 ? colour.brighter (0.15f) : colour);
        g.fillRoundedRectangle (r, 4.0f);

        g.setColour (type == 0 ? Colours::dim : (type == (int) Step::mute ? Colours::text : juce::Colour (0xff111111)));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (type == 0 ? juce::String (i + 1) : juce::String (stepNames[type]), r.reduced (2.0f), juce::Justification::centred);

        if (i == playing)
        {
            g.setColour (Colours::text);
            g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 2.0f);
        }
    }
}

void StepSequencer::timerCallback()
{
    const int now = processor.engine.currentStep.load();
    if (now != playing) { playing = now; repaint(); }
}

void StepSequencer::mouseDown (const juce::MouseEvent& e)
{
    const int index = cellAt (e.position);
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        for (int t = 0; t < numStepTypes; ++t)
            menu.addItem (t + 1, stepNames[t], true, types[(size_t) index] == t);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this, index] (int result) { if (result > 0) setStep (index, result - 1); });
        return;
    }
    paintType = (types[(size_t) index] + (e.mods.isShiftDown() ? numStepTypes - 1 : 1)) % numStepTypes;
    setStep (index, paintType);
    lastPainted = index;
}

void StepSequencer::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const int index = cellAt (e.position);
    if (index != lastPainted)
    {
        setStep (index, paintType);
        lastPainted = index;
    }
}

void StepSequencer::mouseDoubleClick (const juce::MouseEvent& e)
{
    setStep (cellAt (e.position), 0);
}

void StepSequencer::clear()
{
    for (int i = 0; i < numSteps; ++i) setStep (i, 0);
}

// Un motif au hasard qui reste musical : surtout des répétitions, un roll en fin de mesure.
void StepSequencer::randomise()
{
    const int weights[] { 5, 4, 1, 1, 1, 1, 1 };
    for (int i = 0; i < numSteps; ++i)
    {
        int pick = random.nextInt (14), type = 0;
        while (pick >= weights[type]) { pick -= weights[type]; ++type; }
        if (i >= 12 && random.nextFloat() < 0.5f) type = (int) Step::roll;
        setStep (i, type);
    }
}

//==============================================================================
Display::Display (ChopChopProcessor& p) : processor (p)
{
    startTimerHz (30);
}

void Display::timerCallback()
{
    HistoryPoint fresh[256];
    const int n = processor.engine.readHistory (fresh, 256);
    for (int i = 0; i < n; ++i)
    {
        points[(size_t) head] = fresh[i];
        head = (head + 1) % numPoints;
    }
    recentDrums = 0.0f;
    for (int i = 1; i <= 200; ++i)
        recentDrums = std::max (recentDrums, points[(size_t) ((head - i + numPoints) % numPoints)].drums);
    repaint();
}

void Display::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Colours::screen);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto area = bounds.reduced (12.0f, 8.0f);
    const auto band = area.withHeight (6.0f);
    const auto wave = area.withTrimmedTop (12.0f).withTrimmedBottom (8.0f);
    auto xAt = [&] (int i) { return area.getX() + area.getWidth() * (float) i / (float) (numPoints - 1); };
    auto at = [&] (int i) -> const HistoryPoint& { return points[(size_t) ((head + i) % numPoints)]; };
    auto level = [&] (float linear) { return juce::jlimit (0.0f, 1.0f, (gainToDb (linear) + 54.0f) / 54.0f) * wave.getHeight() * 0.5f; };

    // Ce qui joue : une bande colorée par type de pas.
    for (int i = 0; i < numPoints; ++i)
        if (at (i).state > 0)
        {
            g.setColour (Colours::forStep (at (i).state));
            g.fillRect (juce::Rectangle<float> (xAt (i), band.getY(), area.getWidth() / numPoints + 1.0f, band.getHeight()));
        }

    // Voix d'entrée (gris, en miroir) et sortie (blanc).
    juce::Path input, output;
    const float centre = wave.getCentreY();
    input.startNewSubPath (area.getX(), centre);
    for (int i = 0; i < numPoints; ++i) input.lineTo (xAt (i), centre - level (at (i).input));
    for (int i = numPoints - 1; i >= 0; --i) input.lineTo (xAt (i), centre + level (at (i).input));
    input.closeSubPath();
    g.setColour (Colours::main.withAlpha (0.45f));
    g.fillPath (input);

    for (int i = 0; i < numPoints; ++i)
    {
        const float y = centre - level (at (i).output);
        if (i == 0) output.startNewSubPath (xAt (i), y); else output.lineTo (xAt (i), y);
    }
    g.setColour (Colours::text);
    g.strokePath (output, juce::PathStrokeType (1.6f));

    // Drums (orange) et coups détectés.
    juce::Path drums;
    for (int i = 0; i < numPoints; ++i)
    {
        const float y = centre + level (at (i).drums);
        if (i == 0) drums.startNewSubPath (xAt (i), y); else drums.lineTo (xAt (i), y);
    }
    g.setColour (Colours::accent.withAlpha (0.8f));
    g.strokePath (drums, juce::PathStrokeType (1.2f));
    for (int i = 0; i < numPoints; ++i)
        if (at (i).hit)
            g.fillRect (juce::Rectangle<float> (xAt (i) - 1.0f, wave.getBottom() + 1.0f, 2.5f, 6.0f));

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (Colours::dim);
    g.drawText ("VOICE IN", area.withTrimmedTop (12.0f).reduced (6.0f, 0.0f), juce::Justification::topLeft);
    g.setColour (Colours::text);
    g.drawText ("OUT", area.withTrimmedTop (12.0f).reduced (76.0f, 0.0f).withX (area.getX() + 80.0f), juce::Justification::topLeft);
    g.setColour (Colours::accent);
    g.drawText ("DRUMS", area.withTrimmedTop (12.0f).withX (area.getX() + 120.0f), juce::Justification::topLeft);

    // Aide selon le déclenchement.
    const auto p = processor.engine.currentParams();
    juce::String hint;
    if (p.trigger == Trigger::drums && (! processor.drumsConnected() || recentDrums < 1.0e-4f))
        hint = "No drums: enable the plugin's side-chain input and route the drum track to it";
    else if (p.trigger == Trigger::drums)
        hint = "Each drum hit plays the next active step";
    else if (p.trigger == Trigger::midi)
        hint = "Each MIDI note replays the last slice at the note's pitch (C3 = original)";
    if (hint.isNotEmpty())
    {
        g.setColour (Colours::accent.withAlpha (0.85f));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (hint, area.reduced (6.0f, 2.0f).withTrimmedTop (12.0f), juce::Justification::topRight);
    }
}

//==============================================================================
class ChopChopEditor::Content final : public juce::Component
{
public:
    explicit Content (ChopChopProcessor& p)
        : processor (p),
          trigger (*p.state.getParameter ("trigger"), { "PATTERN", "DRUMS", "MIDI" }),
          display (p), sequencer (p),
          size (p.state, "size", "SIZE"), rise (p.state, "rise", "RISE"), decay (p.state, "decay", "DECAY"),
          chance (p.state, "chance", "CHANCE"), pitch (p.state, "pitch", "PITCH"), snap (p.state, "snap", "SNAP"),
          stepRate (p.state, "stepRate", "STEP"),
          band (p.state, "band", "BAND"), sensitivity (p.state, "sensitivity", "THRESH"), hitLength (p.state, "hitLength", "LENGTH"),
          pump (p.state, "pump", "PUMP"), pumpRelease (p.state, "pumpRelease", "RELEASE"),
          mix (p.state, "mix", "MIX"), output (p.state, "output", "OUTPUT"),
          dirt (p.state, "dirt", "TYPE"), drive (p.state, "drive", "DRIVE"), drumDrive (p.state, "drumDrive", "DRUM DRIVE"),
          filter (p.state, "filter", "TYPE"), cutoff (p.state, "cutoff", "CUTOFF"), resonance (p.state, "resonance", "RES"),
          filterMod (p.state, "filterMod", "DRUM MOD"),
          delayTime (p.state, "delayDivision", "TIME"), feedback (p.state, "feedback", "FEEDBACK"), echo (p.state, "echo", "ECHO"),
          room (p.state, "reverbSize", "ROOM"), reverb (p.state, "reverb", "REVERB")
    {
        for (auto* c : std::initializer_list<juce::Component*> { &trigger, &display, &sequencer, &size, &rise, &decay, &chance, &pitch,
                                                                   &snap, &stepRate, &band, &sensitivity, &hitLength, &pump, &pumpRelease,
                                                                   &mix, &output, &dirt, &drive, &drumDrive, &filter, &cutoff, &resonance,
                                                                   &filterMod, &delayTime, &feedback, &echo, &room, &reverb,
                                                                   &presets, &clearButton, &randomButton })
            addAndMakeVisible (c);

        for (int i = 0; i < p.getNumPrograms(); ++i)
            presets.addItem (p.getProgramName (i), i + 1);
        presets.setSelectedId (p.getCurrentProgram() + 1, juce::dontSendNotification);
        presets.onChange = [this] { processor.setCurrentProgram (presets.getSelectedId() - 1); };

        clearButton.onClick = [this] { sequencer.clear(); };
        randomButton.onClick = [this] { sequencer.randomise(); };

        setSize (designWidth, designHeight);
        refresh();
    }

    void refresh()
    {
        const auto t = (Trigger) trigger.selected();
        stepRate.setVisible (t == Trigger::pattern);
        for (auto* c : { &band, &sensitivity, &hitLength })
            c->setVisible (t == Trigger::drums);
        if (presets.getSelectedId() != processor.getCurrentProgram() + 1)
            presets.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Colours::panel);
        g.setColour (Colours::wood);
        g.fillRect (0, 0, 14, getHeight());
        g.fillRect (getWidth() - 14, 0, 14, getHeight());

        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (28.0f, juce::Font::italic));
        g.drawText ("Chop Chop", 30, 8, 220, 34, juce::Justification::centredLeft);
        g.setColour (Colours::dim);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText ("VOCAL REPEATER", 200, 8, 200, 34, juce::Justification::centredLeft);
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("L O A D   &   P L A Y", getWidth() - 230, 8, 200, 34, juce::Justification::centredRight);
        g.setColour (Colours::line);
        g.drawHorizontalLine (48, 30.0f, (float) getWidth() - 30.0f);

        g.setColour (Colours::dim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("TRIGGER", 30, 56, 70, 30, juce::Justification::centredLeft);

        const auto t = (Trigger) trigger.selected();
        section (g, "REPEAT", 30, 282, 420);
        section (g, t == Trigger::pattern ? "PATTERN" : (t == Trigger::drums ? "DRUMS" : "MIDI"), 470, 282, 210);
        section (g, "PUMP", 700, 282, 140);
        section (g, "OUTPUT", 860, 282, 150);
        section (g, "DIRT", 30, 422, 210);
        section (g, "FILTER", 260, 422, 280);
        section (g, "SPACE", 560, 422, 450);

        g.setColour (Colours::dim);
        g.setFont (juce::FontOptions (12.0f));
        if (t == Trigger::pattern)
            g.drawFittedText ("16 steps locked to the\nproject tempo.", juce::Rectangle<int> (545, 330, 135, 60), juce::Justification::centredLeft, 3);
        else if (t == Trigger::midi)
            g.drawFittedText ("Route a MIDI track to the plugin\nand play the slice on a keyboard.\nC3 = original pitch.",
                              juce::Rectangle<int> (470, 320, 210, 80), juce::Justification::centredLeft, 4);
    }

    void resized() override
    {
        presets.setBounds (640, 12, 200, 26);
        trigger.setBounds (100, 56, 330, 30);
        display.setBounds (30, 96, 980, 110);
        sequencer.setBounds (30, 216, 850, 54);
        clearButton.setBounds (890, 216, 120, 25);
        randomButton.setBounds (890, 245, 120, 25);

        auto cell = [] (int x, int y) { return juce::Rectangle<int> (x, y, 70, 112); };
        int x = 30;
        for (auto* c : std::initializer_list<juce::Component*> { &size, &rise, &decay, &chance, &pitch, &snap })
        { c->setBounds (cell (x, 298)); x += 70; }
        stepRate.setBounds (cell (470, 298));
        band.setBounds (cell (470, 298));
        sensitivity.setBounds (cell (540, 298));
        hitLength.setBounds (cell (610, 298));
        pump.setBounds (cell (700, 298));
        pumpRelease.setBounds (cell (770, 298));
        mix.setBounds (cell (860, 298));
        output.setBounds (cell (930, 298));

        x = 30;
        for (auto* c : std::initializer_list<juce::Component*> { &dirt, &drive, &drumDrive })
        { c->setBounds (cell (x, 438)); x += 70; }
        x = 260;
        for (auto* c : std::initializer_list<juce::Component*> { &filter, &cutoff, &resonance, &filterMod })
        { c->setBounds (cell (x, 438)); x += 70; }
        x = 560;
        for (auto* c : std::initializer_list<juce::Component*> { &delayTime, &feedback, &echo, &room, &reverb })
        { c->setBounds (cell (x, 438)); x += 70; }
    }

private:
    static void section (juce::Graphics& g, const juce::String& title, int x, int y, int width)
    {
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (title, x, y, width, 16, juce::Justification::centredLeft);
        g.setColour (Colours::line);
        g.drawHorizontalLine (y + 17, (float) x, (float) (x + width));
    }

    ChopChopProcessor& processor;
    Segmented trigger;
    Display display;
    StepSequencer sequencer;
    Knob size, rise, decay, chance, pitch;
    Switch snap;
    Knob stepRate, band, sensitivity, hitLength, pump, pumpRelease, mix, output;
    Knob dirt, drive, drumDrive, filter, cutoff, resonance, filterMod, delayTime, feedback, echo, room, reverb;
    juce::ComboBox presets;
    juce::TextButton clearButton { "CLEAR" }, randomButton { "RANDOM" };
};

//==============================================================================
ChopChopEditor::ChopChopEditor (ChopChopProcessor& p)
    : AudioProcessorEditor (p), content (std::make_unique<Content> (p))
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (*content);
    setResizable (true, true);
    setResizeLimits (designWidth * 3 / 4, designHeight * 3 / 4, designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / designHeight);
    setSize (designWidth, designHeight);
    startTimerHz (10);
}

ChopChopEditor::~ChopChopEditor()
{
    setLookAndFeel (nullptr);
}

void ChopChopEditor::paint (juce::Graphics& g) { g.fillAll (Colours::panel); }

void ChopChopEditor::resized()
{
    content->setTransform (juce::AffineTransform::scale ((float) getWidth() / designWidth));
}

void ChopChopEditor::timerCallback() { content->refresh(); }
