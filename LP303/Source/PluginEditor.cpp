#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    namespace Colours
    {
        const juce::Colour panel      { 0xff161616 };
        const juce::Colour text       { 0xfff0ede4 };
        const juce::Colour dimText    { 0xffa39e94 };
        const juce::Colour ink        { 0xff1e1e1e };     // inscriptions sur l'alu
        const juce::Colour inkDim     { 0xff55534f };
        const juce::Colour silverHi   { 0xffdcdcd8 };
        const juce::Colour silverLo   { 0xffb4b4af };
        const juce::Colour woodDark   { 0xff4a2a14 };
        const juce::Colour woodLight  { 0xff7b4a26 };
        const juce::Colour accent     { 0xffe8862c };     // orange Load & Play
        const juce::Colour ledOn      { 0xffff3326 };
        const juce::Colour ledOff     { 0xff4a1512 };
        const juce::Colour cream      { 0xffe9dfc2 };
    }

    constexpr int woodWidth = 22;
    constexpr int left = woodWidth + 14;
    constexpr int designWidth = 1180;

    constexpr int headerHeight = 70;
    constexpr int knobTop = headerHeight + 8;        // titres de section
    constexpr int knobCellWidth = 96;
    constexpr int knobCellHeight = 150;
    constexpr int seqTop = 252;
    constexpr int seqHeight = 312;
    constexpr int keyboardTop = seqTop + seqHeight;
    constexpr int keyboardHeight = 84;
    constexpr int designHeight = keyboardTop + keyboardHeight;

    constexpr int seqColumnWidth = 250;
    constexpr int gridLeft = left + seqColumnWidth + 14;

    struct SoundSection
    {
        const char* title;
        int numCells;
    };

    // Rangée du son sur l'alu : sections et nombre de cases de chacune.
    const SoundSection soundSections[] = { { "VCO", 2 }, { "FILTER", 4 }, { "ACCENT", 1 }, { "SLIDE", 1 }, { "DRIVE", 1 }, { "OUTPUT", 1 } };
    constexpr int numSoundSections = 6;
    constexpr int numSoundCells = 10;
    constexpr int soundGap = (designWidth - 2 * left - numSoundCells * knobCellWidth) / (numSoundSections - 1);

    juce::Rectangle<int> soundCell (int cellIndex)
    {
        int x = left, cell = 0;
        for (const auto& section : soundSections)
        {
            for (int i = 0; i < section.numCells; ++i, ++cell)
            {
                if (cell == cellIndex)
                    return { x, knobTop + 18, knobCellWidth, knobCellHeight };
                x += knobCellWidth;
            }
            x += soundGap;
        }
        return {};
    }

    // Petites icônes de forme d'onde, dessinées dans un rectangle.
    void drawWaveIcon (juce::Graphics& g, juce::Rectangle<float> r, bool square)
    {
        juce::Path p;
        if (square)
        {
            p.startNewSubPath (r.getX(), r.getBottom());
            p.lineTo (r.getX(), r.getY());
            p.lineTo (r.getCentreX(), r.getY());
            p.lineTo (r.getCentreX(), r.getBottom());
            p.lineTo (r.getRight(), r.getBottom());
            p.lineTo (r.getRight(), r.getY());
        }
        else
        {
            p.startNewSubPath (r.getX(), r.getBottom());
            p.lineTo (r.getCentreX(), r.getY());
            p.lineTo (r.getCentreX(), r.getBottom());
            p.lineTo (r.getRight(), r.getY());
            p.lineTo (r.getRight(), r.getBottom());
        }
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }

    const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C" };
    bool isBlackKey (int note) { return note == 1 || note == 3 || note == 6 || note == 8 || note == 10; }
}

//==============================================================================
PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b2b2b));
    setColour (juce::TextButton::buttonOnColourId, Colours::accent);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff070707));
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff4a4a4a));
    setColour (juce::ComboBox::textColourId, Colours::ledOn);
    setColour (juce::ComboBox::arrowColourId, Colours::dimText);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1a1a));
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
}

void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const bool silver = (bool) slider.getProperties()["silver"];
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 8.0f;

    // Graduations : 11 traits pour un réglage continu, une par cran sinon.
    const auto range = slider.getRange();
    const int numTicks = slider.getInterval() >= 1.0 ? juce::jmin (25, juce::roundToInt (range.getLength()) + 1) : 11;
    const int majorEvery = numTicks > 13 ? 6 : 1;
    g.setColour (silver ? Colours::inkDim : juce::Colour (0xff8a8780));
    for (int i = 0; i < numTicks; ++i)
    {
        const float angle = startAngle + (endAngle - startAngle) * (float) i / (float) (numTicks - 1);
        const bool major = i % majorEvery == 0 || i == numTicks - 1;
        g.drawLine ({ centre.getPointOnCircumference (radius + 3.0f, angle),
                      centre.getPointOnCircumference (radius + (major ? 7.5f : 5.0f), angle) }, major ? 1.3f : 0.9f);
    }

    // Ombre portée sur l'alu.
    if (silver)
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre.translated (1.5f, 2.5f)));
    }

    // Jupe noire striée.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3c3c3c), centre.translated (-radius * 0.6f, -radius * 0.6f),
                                             juce::Colour (0xff050505), centre.translated (radius, radius), true));
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));

    g.setColour (juce::Colour (0xff4a4a4a));
    for (int i = 0; i < 28; ++i)
    {
        const float angle = juce::MathConstants<float>::twoPi * (float) i / 28.0f;
        g.drawLine ({ centre.getPointOnCircumference (radius * 0.86f, angle),
                      centre.getPointOnCircumference (radius * 0.98f, angle) }, 0.8f);
    }

    // Dessus bombé.
    const float top = radius * 0.78f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a4a), centre.translated (-top * 0.7f, -top * 0.8f),
                                             juce::Colour (0xff0d0d0d), centre.translated (top * 0.6f, top * 0.8f), true));
    g.fillEllipse (juce::Rectangle<float> (top * 2.0f, top * 2.0f).withCentre (centre));

    // Index blanc.
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    g.setColour (Colours::text);
    g.drawLine ({ centre.getPointOnCircumference (radius * 0.28f, angle),
                  centre.getPointOnCircumference (radius * 0.92f, angle) }, 2.6f);
}

void PanelLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down)
{
    // Gros bouton carré en caoutchouc, LED rouge au-dessus.
    auto bounds = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();

    const auto led = juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ bounds.getCentreX(), bounds.getY() + 7.0f });
    if (on)
    {
        g.setColour (Colours::ledOn.withAlpha (0.3f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (on ? Colours::ledOn : Colours::ledOff);
    g.fillEllipse (led);

    auto pad = bounds.withTrimmedTop (18.0f).reduced (2.0f);
    if (down)
        pad = pad.reduced (1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a48).brighter (highlighted ? 0.15f : 0.0f), pad.getX(), pad.getY(),
                                             juce::Colour (0xff2a2a29), pad.getX(), pad.getBottom(), false));
    g.fillRoundedRectangle (pad, 4.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (pad, 4.0f, 1.2f);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.1f));
    g.drawText (button.getButtonText(), pad, juce::Justification::centred);
}

void PanelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                                             bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = background.brighter (highlighted ? 0.12f : 0.0f);
    if (down)
        colour = colour.darker (0.2f);
    g.setColour (colour);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colour (0xff4a4a4a));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

juce::Font PanelLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (13.0f, (float) buttonHeight * 0.55f), juce::Font::bold));
}

//==============================================================================
LedChoice::LedChoice (juce::RangedAudioParameter& parameter, juce::StringArray l, int numColumns, bool onSilver)
    : labels (std::move (l)),
      columns (juce::jmax (1, numColumns)),
      silver (onSilver),
      attachment (parameter, [this] (float v) { index = juce::roundToInt (v); repaint(); })
{
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> LedChoice::cellBounds (int i) const
{
    const int rows = (labels.size() + columns - 1) / columns;
    const float w = (float) getWidth() / (float) columns;
    const float h = (float) getHeight() / (float) rows;
    return { (float) (i % columns) * w, (float) (i / columns) * h, w, h };
}

void LedChoice::setPlaying (int i)
{
    if (i != playing)
    {
        playing = i;
        repaint();
    }
}

void LedChoice::paint (juce::Graphics& g)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        const auto cell = cellBounds (i).reduced (2.0f);
        const bool selected = i == index;

        // Touche carrée, LED à gauche du libellé.
        g.setColour (silver ? juce::Colour (0xff2a2a29) : juce::Colour (0xff262626));
        g.fillRoundedRectangle (cell, 3.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (cell, 3.0f, 1.0f);

        const auto led = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ cell.getX() + 10.0f, cell.getCentreY() });
        if (selected)
        {
            g.setColour (Colours::ledOn.withAlpha (0.3f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (selected ? Colours::ledOn : Colours::ledOff);
        g.fillEllipse (led);

        auto textArea = cell.withTrimmedLeft (18.0f);
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));

        if (labels[i] == "SAW" || labels[i] == "SQUARE")
        {
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (labels[i], textArea.withTrimmedRight (26.0f), juce::Justification::centred);
            drawWaveIcon (g, juce::Rectangle<float> (13.0f, 9.0f).withCentre ({ cell.getRight() - 14.0f, cell.getCentreY() }),
                          labels[i] == "SQUARE");
        }
        else
        {
            g.drawText (labels[i], textArea.withTrimmedRight (6.0f), juce::Justification::centred);
        }

        // Motif en cours de lecture : point orange dans le coin.
        if (i == playing)
        {
            g.setColour (Colours::accent);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ cell.getRight() - 6.0f, cell.getY() + 6.0f }));
        }
    }
}

void LedChoice::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < labels.size(); ++i)
        if (cellBounds (i).contains (e.position))
            attachment.setValueAsCompleteGesture ((float) i);
}

//==============================================================================
namespace
{
    constexpr float gutter = 54.0f;
    constexpr float headerRow = 20.0f;
    constexpr float noteRow = 12.0f;
    constexpr float toggleGap = 8.0f;
    constexpr float toggleRow = 19.0f;
    constexpr float helpRow = 22.0f;
}

PatternGrid::PatternGrid (LP303Processor& p) : processor (p)
{
    setRepaintsOnMouseActivity (false);
}

juce::Rectangle<float> PatternGrid::gridArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (gutter).withTrimmedBottom (helpRow);
}

float PatternGrid::stepWidth() const
{
    return gridArea().getWidth() / (float) lp303::numSteps;
}

juce::Rectangle<float> PatternGrid::noteCell (int step, int note) const
{
    const auto area = gridArea();
    return { area.getX() + (float) step * stepWidth(), area.getY() + headerRow + (float) (12 - note) * noteRow, stepWidth(), noteRow };
}

juce::Rectangle<float> PatternGrid::toggleCell (int step, int toggleRow_) const
{
    const auto area = gridArea();
    const float top = area.getY() + headerRow + 13.0f * noteRow + toggleGap;
    return { area.getX() + (float) step * stepWidth(), top + (float) toggleRow_ * toggleRow, stepWidth(), toggleRow };
}

PatternGrid::Hit PatternGrid::findHit (juce::Point<float> pt) const
{
    const auto area = gridArea();
    Hit hit;
    if (pt.x < area.getX() || pt.x >= area.getRight())
        return hit;

    hit.step = juce::jlimit (0, lp303::numSteps - 1, (int) ((pt.x - area.getX()) / stepWidth()));
    const float y = pt.y - area.getY();

    if (y < headerRow)
    {
        hit.row = Row::header;
    }
    else if (y < headerRow + 13.0f * noteRow)
    {
        hit.row = Row::note;
        hit.note = juce::jlimit (0, 12, 12 - (int) ((y - headerRow) / noteRow));
    }
    else
    {
        const int t = (int) ((y - headerRow - 13.0f * noteRow - toggleGap) / toggleRow);
        static const Row rows[] = { Row::up, Row::down, Row::accent, Row::slide };
        if (y >= headerRow + 13.0f * noteRow + toggleGap && t >= 0 && t < numToggleRows)
            hit.row = rows[t];
    }
    return hit;
}

void PatternGrid::apply (const Hit& hit, bool firstClick)
{
    auto& bank = processor.patterns;
    auto step = bank.get (pattern, hit.step);

    switch (hit.row)
    {
        case Row::header:
            if (firstClick)
                bank.setLength (pattern, hit.step + 1);
            break;

        case Row::note:
            if (firstClick)
                dragValue = ! (step.gate && step.note == hit.note);   // re-clic sur la note : silence
            step.gate = dragValue;
            if (dragValue)
                step.note = hit.note;
            break;

        case Row::up:
        case Row::down:
        {
            const int target = hit.row == Row::up ? 1 : -1;
            if (firstClick)
                dragValue = step.octave != target;
            step.octave = dragValue ? target : 0;
            break;
        }

        case Row::accent:
            if (firstClick)
                dragValue = ! step.accent;
            step.accent = dragValue;
            break;

        case Row::slide:
            if (firstClick)
                dragValue = ! step.slide;
            step.slide = dragValue;
            break;

        case Row::none:
            return;
    }

    if (hit.row != Row::header)
        bank.set (pattern, hit.step, step);

    edited = true;
    lastDragStep = hit.step;
    repaint();
}

void PatternGrid::mouseDown (const juce::MouseEvent& e)
{
    const auto hit = findHit (e.position);
    dragRow = hit.row;
    edited = false;
    if (hit.row != Row::none)
        apply (hit, true);
}

void PatternGrid::mouseDrag (const juce::MouseEvent& e)
{
    const auto hit = findHit (e.position);
    if (dragRow == Row::none || dragRow == Row::header || hit.row != dragRow)
        return;

    // Peindre : une seule modification par pas, sauf changement de note.
    const auto current = processor.patterns.get (pattern, hit.step);
    if (hit.step != lastDragStep || (dragRow == Row::note && dragValue && current.note != hit.note))
        apply (hit, false);
}

void PatternGrid::mouseUp (const juce::MouseEvent&)
{
    if (edited)
        processor.patternsEdited();
    dragRow = Row::none;
    lastDragStep = -1;
    edited = false;
}

void PatternGrid::paint (juce::Graphics& g)
{
    const auto& bank = processor.patterns;
    const auto area = gridArea();
    const float sw = stepWidth();
    const int length = bank.getLength (pattern);

    g.setColour (juce::Colour (0xff0c0c0c));
    g.fillRoundedRectangle (getLocalBounds().toFloat().withTrimmedBottom (helpRow), 4.0f);

    // Lignes de notes façon piano-roll, libellés à gauche.
    for (int note = 0; note <= 12; ++note)
    {
        const auto row = juce::Rectangle<float> (area.getX(), noteCell (0, note).getY(), area.getWidth(), noteRow);
        g.setColour (isBlackKey (note) ? juce::Colour (0xff131313) : juce::Colour (0xff1d1d1c));
        g.fillRect (row.reduced (0.0f, 0.5f));

        g.setColour (isBlackKey (note) ? Colours::dimText.darker (0.3f) : Colours::dimText);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (juce::String (noteNames[note]) + (note == 12 ? "'" : ""),
                    juce::Rectangle<float> (4.0f, row.getY(), gutter - 10.0f, noteRow), juce::Justification::centredRight);
    }

    // Rangées d'options.
    static const char* toggleNames[] = { "OCT +", "OCT -", "ACCENT", "SLIDE" };
    for (int t = 0; t < numToggleRows; ++t)
    {
        g.setColour (Colours::dimText);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (toggleNames[t], juce::Rectangle<float> (4.0f, toggleCell (0, t).getY(), gutter - 10.0f, toggleRow),
                    juce::Justification::centredRight);
    }

    // Colonne jouée.
    if (juce::isPositiveAndBelow (playingStep, lp303::numSteps))
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (area.getX() + (float) playingStep * sw, area.getY(), sw, area.getHeight()));
    }

    // Séparateurs : fin à chaque pas, marqué à chaque temps.
    for (int s = 1; s < lp303::numSteps; ++s)
    {
        g.setColour (s % 4 == 0 ? juce::Colour (0xff4a4a4a) : juce::Colour (0xff262626));
        const float x = area.getX() + (float) s * sw;
        g.drawVerticalLine (juce::roundToInt (x), area.getY() + 2.0f, area.getBottom() - 2.0f);
    }

    // En-tête : numéros des pas, LED de lecture.
    for (int s = 0; s < lp303::numSteps; ++s)
    {
        const auto cell = juce::Rectangle<float> (area.getX() + (float) s * sw, area.getY(), sw, headerRow);
        const bool inPattern = s < length;
        if (s == playingStep)
        {
            g.setColour (Colours::ledOn);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ cell.getX() + 9.0f, cell.getCentreY() }));
        }
        g.setColour (inPattern ? (s % 4 == 0 ? Colours::text : Colours::dimText) : juce::Colour (0xff4a4a4a));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (juce::String (s + 1), cell, juce::Justification::centred);
    }

    // Notes, avec les liaisons de slide.
    for (int s = 0; s < lp303::numSteps; ++s)
    {
        const auto st = bank.get (pattern, s);
        if (! st.gate)
            continue;

        const auto cell = noteCell (s, st.note).reduced (2.0f, 1.0f);
        const auto colour = st.accent ? juce::Colour (0xffff5a36) : Colours::accent;

        if (st.slide && s + 1 < length)
        {
            const auto next = bank.get (pattern, s + 1);
            if (next.gate)
            {
                const auto target = noteCell (s + 1, next.note).reduced (2.0f, 1.0f);
                g.setColour (colour.withAlpha (0.8f));
                g.drawLine (cell.getRight() - 2.0f, cell.getCentreY(), target.getX() + 2.0f, target.getCentreY(), 3.0f);
            }
        }

        g.setColour (colour);
        g.fillRoundedRectangle (cell, 2.5f);

        // Octave : petite flèche dans la note.
        if (st.octave != 0)
        {
            juce::Path arrow;
            const auto c = cell.getCentre();
            const float dir = st.octave > 0 ? -1.0f : 1.0f;
            arrow.addTriangle (c.x - 4.0f, c.y - dir * 2.5f, c.x + 4.0f, c.y - dir * 2.5f, c.x, c.y + dir * 3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.fillPath (arrow);
        }
    }

    // Pastilles des options.
    for (int s = 0; s < lp303::numSteps; ++s)
    {
        const auto st = bank.get (pattern, s);
        const bool states[] = { st.octave > 0, st.octave < 0, st.accent, st.slide };
        const juce::Colour onColours[] = { Colours::cream, Colours::cream, Colours::ledOn, Colours::accent };

        for (int t = 0; t < numToggleRows; ++t)
        {
            const auto pad = toggleCell (s, t).reduced (4.0f, 3.0f);
            g.setColour (states[t] ? onColours[t] : juce::Colour (0xff2a2a2a));
            g.fillRoundedRectangle (pad, 2.5f);
            if (! states[t])
            {
                g.setColour (juce::Colour (0xff383838));
                g.drawRoundedRectangle (pad, 2.5f, 1.0f);
            }
        }
    }

    // Pas hors du motif : assombris.
    if (length < lp303::numSteps)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<float> (area.getX() + (float) length * sw, area.getY() + headerRow,
                                            (float) (lp303::numSteps - length) * sw, area.getHeight() - headerRow));
    }

    // Mode d'emploi.
    g.setColour (Colours::dimText.darker (0.2f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String::fromUTF8 ("Clic : note (re-clic : silence)   \xc2\xb7   glisser pour peindre   \xc2\xb7   "
                                        "clic sur un num\xc3\xa9ro : longueur du motif"),
                getLocalBounds().toFloat().removeFromBottom (helpRow - 4.0f), juce::Justification::centredRight);
}

//==============================================================================
LP303Editor::LP303Editor (LP303Processor& p)
    : AudioProcessorEditor (p),
      processorRef (p),
      grid (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (panel);
    panel.setSize (designWidth, designHeight);

    //--------------------------------------------------------------------------
    // Son, sur l'alu.
    auto place = [] (Knob& k, juce::Rectangle<int> cell) { k.slider.setBounds (cell.withTrimmedTop (16).reduced (2, 0)); };

    place (addKnob ("tune", "TUNE", true), soundCell (0));
    waveChoice = std::make_unique<LedChoice> (*processorRef.apvts.getParameter ("wave"), juce::StringArray { "SAW", "SQUARE" }, 1, true);
    panel.addAndMakeVisible (*waveChoice);
    waveChoice->setBounds (soundCell (1).withTrimmedTop (34).withHeight (66).reduced (4, 0));

    place (addKnob ("cutoff", "CUTOFF", true), soundCell (2));
    place (addKnob ("resonance", "RESONANCE", true), soundCell (3));
    place (addKnob ("envMod", "ENV MOD", true), soundCell (4));
    place (addKnob ("decay", "DECAY", true), soundCell (5));
    place (addKnob ("accent", "AMOUNT", true), soundCell (6));
    place (addKnob ("slide", "TIME", true), soundCell (7));
    place (addKnob ("drive", "AMOUNT", true), soundCell (8));
    place (addKnob ("volume", "VOLUME", true), soundCell (9));

    //--------------------------------------------------------------------------
    // Séquenceur.
    runAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.apvts, "seqOn", runButton);
    panel.addAndMakeVisible (runButton);
    runButton.setBounds (left, seqTop + 36, 76, 66);

    juce::StringArray patternLabels;
    for (int i = 1; i <= lp303::numPatterns; ++i)
        patternLabels.add (juce::String (i));
    patternChoice = std::make_unique<LedChoice> (*processorRef.apvts.getParameter ("pattern"), patternLabels, 4, false);
    panel.addAndMakeVisible (*patternChoice);
    patternChoice->setBounds (left + 88, seqTop + 40, seqColumnWidth - 88, 62);

    const int lengthY = seqTop + 120;
    lengthDown.setBounds (left + 88, lengthY, 30, 24);
    lengthUp.setBounds (left + seqColumnWidth - 30, lengthY, 30, 24);
    lengthDown.onClick = [this] { editPattern ([] (lp303::PatternBank& b, int pat) { b.setLength (pat, b.getLength (pat) - 1); }); };
    lengthUp.onClick = [this] { editPattern ([] (lp303::PatternBank& b, int pat) { b.setLength (pat, b.getLength (pat) + 1); }); };

    const int knobY = seqTop + 156;
    const char* seqKnobs[][2] = { { "swing", "SWING" }, { "bpm", "TEMPO" }, { "transpose", "TRANSPOSE" } };
    for (int i = 0; i < 3; ++i)
    {
        auto& k = addKnob (seqKnobs[i][0], seqKnobs[i][1], false);
        k.slider.setBounds (left + i * (seqColumnWidth / 3), knobY + 14, seqColumnWidth / 3, 84);
    }

    const int buttonsY = seqTop + 266;
    const int buttonWidth = (seqColumnWidth - 3 * 6) / 4;
    juce::TextButton* tools[] = { &clearButton, &randomButton, &copyButton, &pasteButton };
    for (int i = 0; i < 4; ++i)
        tools[i]->setBounds (left + i * (buttonWidth + 6), buttonsY, buttonWidth, 26);

    clearButton.onClick = [this] { editPattern ([] (lp303::PatternBank& b, int pat) { b.clear (pat); }); };
    randomButton.onClick = [this]
    {
        editPattern ([] (lp303::PatternBank& b, int pat)
        {
            juce::Random random;
            lp303::randomPattern (b, pat, random);
        });
    };
    copyButton.onClick = [this] { clipboard = processorRef.patterns.toString (editedPattern()); };
    pasteButton.onClick = [this]
    {
        if (clipboard.isNotEmpty())
            editPattern ([this] (lp303::PatternBank& b, int pat) { b.fromString (pat, clipboard); });
    };

    for (auto* b : { &lengthDown, &lengthUp, &clearButton, &randomButton, &copyButton, &pasteButton })
        panel.addAndMakeVisible (b);

    panel.addAndMakeVisible (grid);
    grid.setBounds (gridLeft, seqTop + 14, designWidth - left - gridLeft, seqHeight - 22);

    //--------------------------------------------------------------------------
    // Presets, dans l'en-tête.
    for (int i = 0; i < processorRef.getNumPrograms(); ++i)
        presetBox.addItem (juce::String (i + 1).paddedLeft ('0', 2) + "  " + processorRef.getProgramName (i), i + 1);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0 && index != processorRef.getCurrentProgram())
            processorRef.setCurrentProgram (index);
    };
    auto step = [this] (int delta)
    {
        const int count = processorRef.getNumPrograms();
        processorRef.setCurrentProgram ((processorRef.getCurrentProgram() + delta + count) % count);
        refreshPresetBox();
    };
    previousPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (1); };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &previousPreset, &nextPreset })
        panel.addAndMakeVisible (c);

    const auto presetArea = juce::Rectangle<int> (designWidth / 2 - 190, 21, 380, 30);
    previousPreset.setBounds (presetArea.withWidth (30));
    nextPreset.setBounds (presetArea.withLeft (presetArea.getRight() - 30));
    presetBox.setBounds (presetArea.reduced (36, 0));
    refreshPresetBox();

    //--------------------------------------------------------------------------
    // Clavier : jouer le son, ou transposer le motif pendant que le séquenceur tourne.
    keyboard.setAvailableRange (24, 84);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Colours::accent.withAlpha (0.7f));
    panel.addAndMakeVisible (keyboard);
    keyboard.setBounds (woodWidth, keyboardTop, designWidth - 2 * woodWidth, keyboardHeight);
    keyboard.setKeyWidth ((float) (designWidth - 2 * woodWidth) / 36.0f);

    // Fenêtre redimensionnable, proportions gardées ; plus petite si l'écran l'exige.
    setResizable (true, true);
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);

    float scale = 1.0f;
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        scale = juce::jmin (1.0f, 0.92f * (float) display->userArea.getWidth() / (float) designWidth,
                            0.88f * (float) display->userArea.getHeight() / (float) designHeight);
    setSize (juce::roundToInt ((float) designWidth * scale), juce::roundToInt ((float) designHeight * scale));

    timerCallback();
    startTimerHz (30);
}

LP303Editor::~LP303Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

LP303Editor::Knob& LP303Editor::addKnob (const char* paramId, const juce::String& title, bool onSilver)
{
    auto knob = std::make_unique<Knob>();
    knob->title = title;
    knob->slider.getProperties().set ("silver", onSilver);
    knob->slider.setColour (juce::Slider::textBoxTextColourId, onSilver ? Colours::ink : Colours::dimText);
    knob->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    knob->slider.setColour (juce::Slider::textBoxHighlightColourId, Colours::accent.withAlpha (0.4f));
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, knobCellWidth - 4, 16);
    knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, paramId, knob->slider);

    // Double-clic : retour à la valeur par défaut.
    if (auto* parameter = processorRef.apvts.getParameter (paramId))
        knob->slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

    panel.addAndMakeVisible (knob->slider);
    knobs.push_back (std::move (knob));
    return *knobs.back();
}

int LP303Editor::editedPattern() const
{
    return juce::roundToInt (processorRef.apvts.getRawParameterValue ("pattern")->load());
}

void LP303Editor::editPattern (const std::function<void (lp303::PatternBank&, int)>& change)
{
    change (processorRef.patterns, editedPattern());
    processorRef.patternsEdited();
    grid.repaint();
    panel.repaint (left, seqTop + 110, seqColumnWidth, 40);
}

void LP303Editor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    panel.setTransform (juce::AffineTransform::scale (scale));
}

void LP303Editor::refreshPresetBox()
{
    shownProgram = processorRef.getCurrentProgram();
    presetBox.setSelectedId (shownProgram + 1, juce::dontSendNotification);
}

void LP303Editor::timerCallback()
{
    const int pattern = editedPattern();
    grid.setPattern (pattern);

    const bool running = processorRef.isSequencerRunning();
    const int active = processorRef.getActivePattern();
    grid.setPlayingStep (running && active == pattern ? processorRef.getPlayingStep() : -1);
    patternChoice->setPlaying (running ? active : -1);

    // Les motifs peuvent changer sans l'interface (projet rechargé) : on compare une empreinte.
    static_assert (lp303::numSteps <= 16, "");
    juce::uint64 hash = (juce::uint64) processorRef.patterns.getLength (pattern);
    for (int s = 0; s < lp303::numSteps; ++s)
        hash = hash * 1099511628211ull + processorRef.patterns.get (pattern, s).pack();
    if (hash != gridHash)
    {
        gridHash = hash;
        grid.repaint();
    }

    const int length = processorRef.patterns.getLength (pattern);
    const bool host = processorRef.hasHostTransport();
    if (length != shownLength || host != shownHost)
    {
        shownLength = length;
        shownHost = host;
        panel.repaint (left, seqTop + 100, seqColumnWidth, 50);
    }

    if (processorRef.getCurrentProgram() != shownProgram)
        refreshPresetBox();
}

//==============================================================================
void LP303Editor::paintPanel (juce::Graphics& g)
{
    const auto bounds = panel.getLocalBounds();
    g.fillAll (Colours::panel);

    // Alu brossé : dégradé et fines stries horizontales.
    const auto silver = juce::Rectangle<int> (woodWidth, 0, designWidth - 2 * woodWidth, seqTop);
    g.setGradientFill (juce::ColourGradient (Colours::silverHi, 0.0f, 0.0f, Colours::silverLo, 0.0f, (float) seqTop, false));
    g.fillRect (silver);
    {
        juce::Random grain (303);
        for (int y = 0; y < seqTop; ++y)
        {
            const float a = grain.nextFloat() * 0.07f;
            g.setColour ((grain.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (a));
            g.drawHorizontalLine (y, (float) silver.getX(), (float) silver.getRight());
        }
    }
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawHorizontalLine (seqTop - 1, (float) silver.getX(), (float) silver.getRight());
    g.setColour (Colours::accent);
    g.fillRect (silver.getX(), seqTop, silver.getWidth(), 3);

    // Joues en bois.
    auto paintWood = [&g, &bounds] (juce::Rectangle<int> area)
    {
        juce::ColourGradient wood (Colours::woodDark, (float) area.getX(), 0.0f, Colours::woodDark, (float) area.getRight(), 0.0f, false);
        wood.addColour (0.5, Colours::woodLight);
        g.setGradientFill (wood);
        g.fillRect (area);

        juce::Random grain (area.getX() + 11);
        g.setColour (Colours::woodDark.withAlpha (0.5f));
        for (int i = 0; i < 9; ++i)
        {
            const float gx = (float) area.getX() + 3.0f + grain.nextFloat() * (float) (area.getWidth() - 6);
            g.drawLine (gx, 0.0f, gx + grain.nextFloat() * 4.0f - 2.0f, (float) bounds.getHeight(), 0.8f);
        }
    };
    paintWood (bounds.withWidth (woodWidth));
    paintWood (bounds.withLeft (bounds.getRight() - woodWidth));

    paintHeader (g);

    // Sections du son : titre encadré de filets, libellés au-dessus des boutons.
    int cell = 0;
    for (const auto& section : soundSections)
    {
        const auto first = soundCell (cell);
        const auto last = soundCell (cell + section.numCells - 1);
        const auto titleArea = juce::Rectangle<int> (first.getX(), knobTop, last.getRight() - first.getX(), 14);

        g.setFont (juce::FontOptions (11.5f, juce::Font::bold).withKerningFactor (0.15f));
        const int textWidth = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), section.title) + 14;
        g.setColour (Colours::ink);
        g.drawText (section.title, titleArea, juce::Justification::centred);
        g.setColour (Colours::inkDim);
        const float midY = (float) titleArea.getCentreY();
        g.drawLine ((float) titleArea.getX() + 4.0f, midY, (float) (titleArea.getCentreX() - textWidth / 2), midY, 1.0f);
        g.drawLine ((float) (titleArea.getCentreX() + textWidth / 2), midY, (float) titleArea.getRight() - 4.0f, midY, 1.0f);

        cell += section.numCells;
    }

    g.setColour (Colours::ink);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    for (const auto& knob : knobs)
        if ((bool) knob->slider.getProperties()["silver"])
            g.drawText (knob->title, knob->slider.getBounds().withY (knob->slider.getY() - 16).withHeight (14),
                        juce::Justification::centred);
    g.drawText ("WAVEFORM", soundCell (1).withHeight (14), juce::Justification::centred);

    // Colonne du séquenceur.
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold).withKerningFactor (0.15f));
    g.drawText ("SEQUENCER", left, seqTop + 12, seqColumnWidth, 16, juce::Justification::centredLeft);

    g.setColour (Colours::dimText);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (shownHost ? "SYNC HOST" : "INTERNAL", left + 120, seqTop + 12, seqColumnWidth - 120, 16, juce::Justification::centredRight);
    g.drawText ("PATTERN", left + 88, seqTop + 26, 100, 14, juce::Justification::centredLeft);
    g.drawText ("LENGTH", left, seqTop + 120, 80, 24, juce::Justification::centredLeft);

    // Afficheur de longueur, façon LED.
    const auto lengthDisplay = juce::Rectangle<int> (left + 124, seqTop + 120, seqColumnWidth - 160, 24);
    g.setColour (juce::Colour (0xff070707));
    g.fillRoundedRectangle (lengthDisplay.toFloat(), 3.0f);
    g.setColour (juce::Colour (0xff4a4a4a));
    g.drawRoundedRectangle (lengthDisplay.toFloat(), 3.0f, 1.0f);
    g.setColour (Colours::ledOn);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (juce::String (shownLength) + (shownLength > 1 ? " STEPS" : " STEP"), lengthDisplay, juce::Justification::centred);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (const auto& knob : knobs)
        if (! (bool) knob->slider.getProperties()["silver"])
            g.drawText (knob->title, knob->slider.getBounds().withY (knob->slider.getY() - 14).withHeight (14),
                        juce::Justification::centred);

    // Filet sous le séquenceur, au-dessus du clavier.
    g.setColour (juce::Colour (0xff3a3a3a));
    g.drawHorizontalLine (keyboardTop - 4, (float) left, (float) (designWidth - left));
}

// En-tête sur l'alu : marque encadrée à gauche, presets au centre, nom du
// modèle en grosses lettres noires à droite.
void LP303Editor::paintHeader (juce::Graphics& g)
{
    const auto header = juce::Rectangle<int> (left, 0, designWidth - 2 * left, headerHeight);

    const auto brand = juce::Rectangle<float> ((float) left, 21.0f, 180.0f, 30.0f);
    g.setColour (Colours::ink);
    g.drawRoundedRectangle (brand, 3.0f, 1.8f);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold).withKerningFactor (0.2f));
    g.drawText ("LOAD & PLAY", brand.toNearestInt(), juce::Justification::centred);

    // Nom du modèle : lettres larges, avec une barre orange en dessous.
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (juce::Font (juce::FontOptions (50.0f, juce::Font::bold).withHorizontalScale (1.25f)), "LP-303", 0.0f, 0.0f);
    juce::Path title;
    glyphs.createPath (title);
    const auto target = juce::Rectangle<float> ((float) header.getRight() - 240.0f, 13.0f, 240.0f, 37.0f);
    title.applyTransform (juce::RectanglePlacement (juce::RectanglePlacement::xRight | juce::RectanglePlacement::yMid)
                              .getTransformToFit (title.getBounds(), target));
    g.setColour (Colours::ink);
    g.fillPath (title);

    const auto titleBounds = title.getBounds();
    g.setColour (Colours::accent);
    g.fillRect (juce::Rectangle<float> (titleBounds.getX(), titleBounds.getBottom() + 4.0f, titleBounds.getWidth(), 3.0f));

    g.setColour (Colours::inkDim);
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold).withKerningFactor (0.18f));
    g.drawText ("ACID BASS SYNTHESIZER WITH 16-STEP SEQUENCER",
                juce::Rectangle<int> (left, 54, 420, 12), juce::Justification::centredLeft);

    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawHorizontalLine (headerHeight, (float) header.getX(), (float) header.getRight());
}
