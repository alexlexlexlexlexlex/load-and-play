#include "PluginEditor.h"

using namespace duckhunt;

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
juce::Font PanelLookAndFeel::getLabelFont (juce::Label&) { return juce::FontOptions (12.0f); }
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

//==============================================================================
Display::Display (DuckHuntProcessor& p) : processor (p)
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

    recentSide = 0.0f;
    for (int i = 1; i <= 200; ++i)
        recentSide = std::max (recentSide, points[(size_t) ((head - i + numPoints) % numPoints)].side);

    if (processor.engine.currentParams().mode == Mode::spectral)
    {
        processor.engine.readSpectrum (spectrum);
        for (int i = 0; i < SpectrumView::numPoints; ++i)
        {
            auto ease = [] (float& state, float target, float k) { state += (target - state) * k; };
            ease (smoothed.mainDb[(size_t) i], spectrum.mainDb[(size_t) i], 0.35f);
            ease (smoothed.sideDb[(size_t) i], spectrum.sideDb[(size_t) i], 0.35f);
            ease (smoothed.reductionDb[(size_t) i], spectrum.reductionDb[(size_t) i], 0.5f);
        }
    }
    repaint();
}

void Display::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Colours::screen);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto p = processor.engine.currentParams();
    const auto area = bounds.reduced (12.0f, 10.0f).withTrimmedLeft (30.0f);

    if (p.mode == Mode::spectral)
        paintSpectrum (g, area, p);
    else
        paintTime (g, area, p);

    // Aide quand rien ne déclenche le duck.
    juce::String hint;
    if (p.trigger == Trigger::sidechain && (! processor.sidechainConnected() || recentSide < 1.0e-4f))
        hint = "No sidechain signal: enable the plugin's side-chain input and route the reference track to it";
    else if (p.trigger == Trigger::midi)
        hint = "MIDI: each note sent to the plugin triggers the duck";
    else if (p.trigger == Trigger::sync)
        hint = juce::String ("Tempo: one duck every ") + divisionNames[(size_t) juce::jlimit (0, 9, p.division)];

    if (hint.isNotEmpty())
    {
        g.setColour (Colours::accent.withAlpha (0.85f));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (hint, area.reduced (8.0f, 4.0f).removeFromBottom (18.0f), juce::Justification::centredRight);
    }
}

void Display::paintTime (juce::Graphics& g, juce::Rectangle<float> area, const Params& p)
{
    const float maxReduction = std::max (12.0f, std::ceil (std::max (p.depthDb, 1.0f) / 12.0f) * 12.0f);
    auto levelY = [&] (float linear) { return area.getBottom() - juce::jlimit (0.0f, 1.0f, (gainToDb (linear) + 60.0f) / 60.0f) * area.getHeight(); };
    auto reductionY = [&] (float db) { return area.getY() + juce::jlimit (0.0f, 1.0f, db / maxReduction) * area.getHeight(); };
    auto xAt = [&] (int i) { return area.getX() + area.getWidth() * (float) i / (float) (numPoints - 1); };
    auto at = [&] (int i) -> const HistoryPoint& { return points[(size_t) ((head + i) % numPoints)]; };

    // Graduations de la baisse.
    g.setFont (juce::FontOptions (10.0f));
    for (float db = 0.0f; db <= maxReduction + 0.1f; db += maxReduction / 4.0f)
    {
        const float y = reductionY (db);
        g.setColour (Colours::line);
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (Colours::dim);
        g.drawText (db == 0.0f ? "0" : "-" + juce::String (juce::roundToInt (db)), juce::Rectangle<float> (area.getX() - 34.0f, y - 7.0f, 28.0f, 14.0f),
                    juce::Justification::centredRight);
    }

    // La piste (gris) et la référence (orange).
    juce::Path mainPath, sidePath;
    mainPath.startNewSubPath (area.getX(), area.getBottom());
    for (int i = 0; i < numPoints; ++i)
    {
        mainPath.lineTo (xAt (i), levelY (at (i).main));
        if (i == 0) sidePath.startNewSubPath (xAt (i), levelY (at (i).side));
        else sidePath.lineTo (xAt (i), levelY (at (i).side));
    }
    mainPath.lineTo (area.getRight(), area.getBottom());
    mainPath.closeSubPath();
    g.setColour (Colours::main.withAlpha (0.55f));
    g.fillPath (mainPath);
    g.setColour (Colours::accent.withAlpha (0.9f));
    g.strokePath (sidePath, juce::PathStrokeType (1.4f));

    // La baisse de volume, du haut vers le bas.
    const int bands = p.mode == Mode::threeBands ? 3 : 1;
    const juce::Colour colours[] { p.mode == Mode::threeBands ? Colours::low : Colours::text, Colours::mid, Colours::high };
    for (int b = bands - 1; b >= 0; --b)
    {
        juce::Path curve, fill;
        fill.startNewSubPath (area.getX(), area.getY());
        for (int i = 0; i < numPoints; ++i)
        {
            const float y = reductionY (at (i).reductionDb[(size_t) b]);
            if (i == 0) curve.startNewSubPath (xAt (i), y);
            else curve.lineTo (xAt (i), y);
            fill.lineTo (xAt (i), y);
        }
        fill.lineTo (area.getRight(), area.getY());
        fill.closeSubPath();
        g.setColour (colours[b].withAlpha (bands == 1 ? 0.18f : 0.10f));
        g.fillPath (fill);
        g.setColour (colours[b]);
        g.strokePath (curve, juce::PathStrokeType (2.0f));
    }

    // Légende.
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    auto legend = area.reduced (8.0f, 4.0f).removeFromTop (16.0f);
    auto item = [&] (const juce::String& text, juce::Colour c)
    {
        auto r = legend.removeFromLeft (14.0f + juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 16.0f);
        g.setColour (c);
        g.fillRect (r.removeFromLeft (10.0f).withSizeKeepingCentre (10.0f, 3.0f));
        g.drawText (text, r.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
    };
    item ("TRACK", Colours::main.brighter (0.6f));
    item ("SIDECHAIN", Colours::accent);
    if (bands == 1) item ("DUCK", Colours::text);
    else { item ("LOW", Colours::low); item ("MID", Colours::mid); item ("HIGH", Colours::high); }
}

void Display::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> area, const Params& p)
{
    const float maxReduction = std::max (12.0f, std::ceil (std::max (p.depthDb, 1.0f) / 12.0f) * 12.0f);
    auto xAtFreq = [&] (float f) { return area.getX() + area.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
    auto xAt = [&] (int i) { return area.getX() + area.getWidth() * (float) i / (float) (SpectrumView::numPoints - 1); };
    auto levelY = [&] (float db) { return area.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f) * area.getHeight(); };
    auto reductionY = [&] (float db) { return area.getY() + juce::jlimit (0.0f, 1.0f, db / maxReduction) * area.getHeight(); };

    // Zone concernée.
    const float lo = std::min (p.focusLow, p.focusHigh), hi = std::max (p.focusLow, p.focusHigh);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (area.withRight (xAtFreq (std::max (20.0f, lo))));
    g.fillRect (area.withLeft (xAtFreq (std::min (20000.0f, hi))));

    g.setFont (juce::FontOptions (10.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xAtFreq (f);
        g.setColour (Colours::line);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (Colours::dim);
        g.drawText (f < 1000.0f ? juce::String ((int) f) : juce::String ((int) (f / 1000.0f)) + "k",
                    juce::Rectangle<float> (x + 3.0f, area.getBottom() - 14.0f, 30.0f, 12.0f), juce::Justification::centredLeft);
    }
    for (float db = 0.0f; db <= maxReduction + 0.1f; db += maxReduction / 4.0f)
    {
        const float y = reductionY (db);
        g.setColour (Colours::dim);
        g.drawText (db == 0.0f ? "0" : "-" + juce::String (juce::roundToInt (db)), juce::Rectangle<float> (area.getX() - 34.0f, y - 7.0f, 28.0f, 14.0f),
                    juce::Justification::centredRight);
    }

    juce::Path mainPath, sidePath, curve, fill;
    mainPath.startNewSubPath (area.getX(), area.getBottom());
    fill.startNewSubPath (area.getX(), area.getY());
    for (int i = 0; i < SpectrumView::numPoints; ++i)
    {
        const float x = xAt (i);
        mainPath.lineTo (x, levelY (smoothed.mainDb[(size_t) i]));
        const float sy = levelY (smoothed.sideDb[(size_t) i]), ry = reductionY (smoothed.reductionDb[(size_t) i]);
        if (i == 0) { sidePath.startNewSubPath (x, sy); curve.startNewSubPath (x, ry); }
        else { sidePath.lineTo (x, sy); curve.lineTo (x, ry); }
        fill.lineTo (x, ry);
    }
    mainPath.lineTo (area.getRight(), area.getBottom());
    mainPath.closeSubPath();
    fill.lineTo (area.getRight(), area.getY());
    fill.closeSubPath();

    g.setColour (Colours::main.withAlpha (0.55f));
    g.fillPath (mainPath);
    g.setColour (Colours::accent.withAlpha (0.9f));
    g.strokePath (sidePath, juce::PathStrokeType (1.4f));
    g.setColour (Colours::text.withAlpha (0.16f));
    g.fillPath (fill);
    g.setColour (Colours::text);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (Colours::dim);
    g.drawText ("TRACK  /  SIDECHAIN  /  DUCK BY FREQUENCY", area.reduced (8.0f, 4.0f).removeFromTop (16.0f), juce::Justification::centredLeft);
}

//==============================================================================
// Toute la façade, dessinée à sa taille d'origine puis agrandie ou réduite.
class DuckHuntEditor::Content final : public juce::Component
{
public:
    explicit Content (DuckHuntProcessor& p)
        : processor (p),
          trigger (*p.state.getParameter ("trigger"), { "SIDECHAIN", "MIDI", "TEMPO" }),
          mode (*p.state.getParameter ("mode"), { "FULL BAND", "3 BANDS", "SPECTRAL" }),
          display (p),
          threshold (p.state, "threshold", "THRESHOLD"), range (p.state, "range", "RANGE"),
          lowCut (p.state, "scLowCut", "SC LOW CUT"), highCut (p.state, "scHighCut", "SC HIGH CUT"),
          division (p.state, "division", "DIVISION"), velocity (p.state, "velocity", "VELOCITY"),
          attack (p.state, "attack", "ATTACK"), hold (p.state, "hold", "HOLD"),
          release (p.state, "release", "RELEASE"), curve (p.state, "curve", "CURVE"),
          depth (p.state, "depth", "DEPTH"), lookahead (p.state, "lookahead", "LOOKAHEAD"), invert (p.state, "invert", "INVERT"),
          crossLow (p.state, "crossoverLow", "LOW / MID"), crossHigh (p.state, "crossoverHigh", "MID / HIGH"),
          bandLow (p.state, "bandLow", "LOW"), bandMid (p.state, "bandMid", "MID"), bandHigh (p.state, "bandHigh", "HIGH"),
          focusLow (p.state, "focusLow", "FOCUS LOW"), focusHigh (p.state, "focusHigh", "FOCUS HIGH"), smoothing (p.state, "smoothing", "SMOOTHING"),
          mix (p.state, "mix", "MIX"), output (p.state, "output", "OUTPUT"), listen (p.state, "listen", "LISTEN SC")
    {
        for (auto* c : std::initializer_list<juce::Component*> { &trigger, &mode, &display, &threshold, &range, &lowCut, &highCut,
                                                                   &division, &velocity, &attack, &hold, &release, &curve, &depth,
                                                                   &lookahead, &invert, &crossLow, &crossHigh, &bandLow, &bandMid,
                                                                   &bandHigh, &focusLow, &focusHigh, &smoothing, &mix, &output, &listen, &presets })
            addAndMakeVisible (c);

        for (int i = 0; i < p.getNumPrograms(); ++i)
            presets.addItem (p.getProgramName (i), i + 1);
        presets.setSelectedId (p.getCurrentProgram() + 1, juce::dontSendNotification);
        presets.onChange = [this] { processor.setCurrentProgram (presets.getSelectedId() - 1); };
        presets.setJustificationType (juce::Justification::centredLeft);

        setSize (designWidth, designHeight);
        refresh();
    }

    // Montre les réglages utiles selon le déclenchement et le mode.
    void refresh()
    {
        const auto t = (Trigger) trigger.selected();
        const auto m = (Mode) mode.selected();
        for (auto* c : { &threshold, &range, &lowCut, &highCut })
            c->setVisible (t == Trigger::sidechain);
        division.setVisible (t == Trigger::sync);
        velocity.setVisible (t == Trigger::midi);
        lookahead.setEnabled (t == Trigger::sidechain);
        for (auto* c : { &crossLow, &crossHigh, &bandLow, &bandMid, &bandHigh })
            c->setVisible (m == Mode::threeBands);
        for (auto* c : { &focusLow, &focusHigh, &smoothing })
            c->setVisible (m == Mode::spectral);
        listen.setEnabled (t == Trigger::sidechain);

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
        g.drawText ("Duck Hunt", 30, 8, 220, 34, juce::Justification::centredLeft);
        g.setColour (Colours::dim);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText ("SIDECHAIN DUCKER", 200, 8, 200, 34, juce::Justification::centredLeft);
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("L O A D   &   P L A Y", getWidth() - 230, 8, 200, 34, juce::Justification::centredRight);
        g.setColour (Colours::line);
        g.drawHorizontalLine (48, 30.0f, (float) getWidth() - 30.0f);

        g.setColour (Colours::dim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("TRIGGER", 30, 58, 70, 30, juce::Justification::centredLeft);
        g.drawText ("MODE", 460, 58, 60, 30, juce::Justification::centredLeft);

        const auto t = (Trigger) trigger.selected();
        const auto m = (Mode) mode.selected();
        section (g, t == Trigger::sidechain ? "DETECTION" : (t == Trigger::midi ? "MIDI" : "TEMPO"), 30, 334, 296);
        section (g, "ENVELOPE", 342, 334, 296);
        section (g, "DUCK", 654, 334, 236);
        section (g, m == Mode::threeBands ? "BANDS" : (m == Mode::spectral ? "SPECTRAL" : "FULL BAND"), 30, 480, 370);
        section (g, "SHAPE", 416, 480, 222);
        section (g, "OUTPUT", 654, 480, 236);

        if (m == Mode::fullBand)
        {
            g.setColour (Colours::dim);
            g.setFont (juce::FontOptions (12.0f));
            g.drawFittedText ("The whole track is turned down.\nChoose 3 BANDS or SPECTRAL to duck by frequency.",
                              juce::Rectangle<int> (30, 506, 370, 80), juce::Justification::centredLeft, 3);
        }
        if (t == Trigger::midi)
        {
            g.setColour (Colours::dim);
            g.setFont (juce::FontOptions (12.0f));
            g.drawFittedText ("Route a MIDI track to the plugin:\neach note triggers the envelope.\nVELOCITY: harder notes duck deeper.",
                              juce::Rectangle<int> (110, 360, 216, 90), juce::Justification::centredLeft, 4);
        }

        paintShape (g, juce::Rectangle<float> (424.0f, 508.0f, 206.0f, 90.0f));
    }

    void resized() override
    {
        presets.setBounds (560, 12, 190, 26);
        trigger.setBounds (100, 58, 330, 30);
        mode.setBounds (520, 58, 370, 30);
        display.setBounds (30, 100, 860, 220);

        auto cell = [] (int x, int y) { return juce::Rectangle<int> (x, y, 74, 112); };
        threshold.setBounds (cell (30, 352));
        range.setBounds (cell (104, 352));
        lowCut.setBounds (cell (178, 352));
        highCut.setBounds (cell (252, 352));
        division.setBounds (cell (30, 352));
        velocity.setBounds (cell (30, 352));

        attack.setBounds (cell (342, 352));
        hold.setBounds (cell (416, 352));
        release.setBounds (cell (490, 352));
        curve.setBounds (cell (564, 352));

        depth.setBounds (cell (654, 352));
        lookahead.setBounds (cell (728, 352));
        invert.setBounds (cell (812, 352));

        crossLow.setBounds (cell (30, 494));
        crossHigh.setBounds (cell (104, 494));
        bandLow.setBounds (cell (178, 494));
        bandMid.setBounds (cell (252, 494));
        bandHigh.setBounds (cell (326, 494));
        focusLow.setBounds (cell (30, 494));
        focusHigh.setBounds (cell (104, 494));
        smoothing.setBounds (cell (178, 494));

        mix.setBounds (cell (654, 494));
        output.setBounds (cell (728, 494));
        listen.setBounds (cell (812, 494));
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

    // Forme de l'enveloppe : attaque, maintien, relâchement et courbe.
    void paintShape (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const auto p = processor.engine.currentParams();
        g.setColour (Colours::screen);
        g.fillRoundedRectangle (r, 4.0f);
        const auto area = r.reduced (8.0f, 8.0f);
        const float total = std::max (1.0f, p.attackMs + p.holdMs + p.releaseMs) * 1.15f;
        const float exponent = std::pow (2.0f, -2.0f * p.curve);
        auto x = [&] (float ms) { return area.getX() + area.getWidth() * ms / total; };
        auto y = [&] (float amount) { return area.getY() + area.getHeight() * std::pow (juce::jlimit (0.0f, 1.0f, amount), exponent); };

        juce::Path shape;
        shape.startNewSubPath (area.getX(), area.getY());
        for (int i = 0; i <= 40; ++i) shape.lineTo (x (p.attackMs * (float) i / 40.0f), y ((float) i / 40.0f));
        shape.lineTo (x (p.attackMs + p.holdMs), y (1.0f));
        for (int i = 0; i <= 60; ++i) shape.lineTo (x (p.attackMs + p.holdMs + p.releaseMs * (float) i / 60.0f), y (1.0f - (float) i / 60.0f));
        shape.lineTo (area.getRight(), area.getY());

        g.setColour (Colours::accent);
        g.strokePath (shape, juce::PathStrokeType (2.0f));
        g.setColour (Colours::dim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (juce::String (juce::roundToInt (p.attackMs + p.holdMs + p.releaseMs)) + " ms", area, juce::Justification::bottomRight);
    }

    DuckHuntProcessor& processor;
    Segmented trigger, mode;
    Display display;
    Knob threshold, range, lowCut, highCut, division;
    Switch velocity;
    Knob attack, hold, release, curve, depth, lookahead;
    Switch invert;
    Knob crossLow, crossHigh, bandLow, bandMid, bandHigh, focusLow, focusHigh, smoothing, mix, output;
    Switch listen;
    juce::ComboBox presets;
};

//==============================================================================
DuckHuntEditor::DuckHuntEditor (DuckHuntProcessor& p)
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

DuckHuntEditor::~DuckHuntEditor()
{
    setLookAndFeel (nullptr);
}

void DuckHuntEditor::paint (juce::Graphics& g) { g.fillAll (Colours::panel); }

void DuckHuntEditor::resized()
{
    content->setTransform (juce::AffineTransform::scale ((float) getWidth() / designWidth));
}

void DuckHuntEditor::timerCallback() { content->refresh(); }
