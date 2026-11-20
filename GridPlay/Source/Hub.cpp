#include "Hub.h"

namespace gridplay
{
namespace
{
    const char* manualUrl = "https://github.com/alexlexlexlexlexlex/load-and-play/tree/main/GridPlay#readme";
    const char* siteUrl = "https://alexlexlexlexlexlex.github.io/load-and-play/";
}

Hub::Hub (juce::AudioDeviceManager& d, juce::RecentlyOpenedFilesList& r, juce::PropertiesFile& s)
    : devices (d), recent (r), settings (s)
{
    // Pilote et sortie audio, en haut à droite (comme le Hub de Cubase).
    driver.setTooltip ("Pilote audio");
    output.setTooltip ("Sortie audio");
    driver.onChange = [this]
    {
        const auto name = driver.getText();
        if (name.isNotEmpty() && name != devices.getCurrentAudioDeviceType())
            devices.setCurrentAudioDeviceType (name, true);
        refreshDevices();
    };
    output.onChange = [this]
    {
        auto setup = devices.getAudioDeviceSetup();
        if (output.getText().isNotEmpty() && output.getText() != setup.outputDeviceName)
        {
            setup.outputDeviceName = output.getText();
            devices.setAudioDeviceSetup (setup, true);
        }
    };
    audioButton.setButtonText (juce::String::fromUTF8 ("Réglages"));
    audioButton.setTooltip (juce::String::fromUTF8 ("Carte son et appareils MIDI en détail"));
    audioButton.onClick = [this] { if (onAudioSettings) onAudioSettings(); };

    // Colonne de gauche
    projectsButton.setButtonText ("Projets");
    projectsButton.setToggleState (true, juce::dontSendNotification);
    projectsButton.setColour (juce::TextButton::buttonOnColourId, theme::text);
    projectsButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    midiButton.setButtonText ("Audio / MIDI");
    pluginsButton.setButtonText ("Instruments et effets");
    manualButton.setButtonText ("Manuel");
    siteButton.setButtonText ("Site Load & Play");
    midiButton.onClick = [this] { if (onAudioSettings) onAudioSettings(); };
    pluginsButton.onClick = [this] { if (onScanPlugins) onScanPlugins(); };
    manualButton.onClick = [] { juce::URL (manualUrl).launchInDefaultBrowser(); };
    siteButton.onClick = [] { juce::URL (siteUrl).launchInDefaultBrowser(); };
    midiButton.setTooltip ("Choisir la carte son, les claviers et ports MIDI");
    pluginsButton.setTooltip (juce::String::fromUTF8 ("Chercher les instruments et effets VST3, AU, LV2 installés"));
    manualButton.setTooltip ("Le mode d'emploi de Grid Play (en ligne)");

    startup.setButtonText (juce::String::fromUTF8 ("Ouvrir le Hub au démarrage"));
    startup.setToggleState (settings.getBoolValue ("showHub", true), juce::dontSendNotification);
    startup.onClick = [this] { settings.setValue ("showHub", startup.getToggleState()); };

    // Projets
    createButton.setButtonText (juce::String::fromUTF8 ("+   Créer un projet"));
    createButton.setColour (juce::TextButton::buttonColourId, theme::accent);
    createButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    createButton.onClick = [this] { if (onCreate) onCreate(); };
    browseButton.setButtonText (juce::String::fromUTF8 ("Sélectionner un fichier..."));
    browseButton.onClick = [this] { if (onBrowse) onBrowse(); };
    openButton.setButtonText ("Ouvrir");
    openButton.setEnabled (false);
    openButton.onClick = [this] { openRow (list.getSelectedRow()); };

    search.setTextToShowWhenEmpty ("Rechercher", theme::textDim);
    search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0d0d0d));
    search.setColour (juce::TextEditor::outlineColourId, theme::rule);
    search.setFont (theme::font (14.0f));
    search.onTextChange = [this] { filter(); };

    list.setModel (this);
    list.setRowHeight (42);
    list.setColour (juce::ListBox::backgroundColourId, theme::panel);
    list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);

    for (auto* c : std::initializer_list<juce::Component*> { &driver, &output, &audioButton, &projectsButton, &midiButton, &pluginsButton,
                                                             &manualButton, &siteButton, &startup, &createButton, &browseButton,
                                                             &openButton, &search, &list })
        addAndMakeVisible (c);

    devices.addChangeListener (this);
    refreshDevices();
    filter();
    setSize (980, 640);
}

Hub::~Hub()
{
    devices.removeChangeListener (this);
}

void Hub::changeListenerCallback (juce::ChangeBroadcaster*) { refreshDevices(); }

void Hub::refreshDevices()
{
    driver.clear (juce::dontSendNotification);
    int id = 1;
    for (auto* type : devices.getAvailableDeviceTypes())
        driver.addItem (type->getTypeName(), id++);
    driver.setText (devices.getCurrentAudioDeviceType(), juce::dontSendNotification);

    output.clear (juce::dontSendNotification);
    if (auto* type = devices.getCurrentDeviceTypeObject())
    {
        id = 1;
        for (const auto& name : type->getDeviceNames (false))
            output.addItem (name, id++);
    }
    output.setText (devices.getAudioDeviceSetup().outputDeviceName, juce::dontSendNotification);
}

void Hub::filter()
{
    shown.clear();
    recent.removeNonExistentFiles();
    const auto needle = search.getText().trim();
    for (int i = 0; i < recent.getNumFiles(); ++i)
    {
        const auto file = recent.getFile (i);
        if (needle.isEmpty() || file.getFileNameWithoutExtension().containsIgnoreCase (needle))
            shown.add (file);
    }
    list.updateContent();
    list.repaint();
    openButton.setEnabled (list.getSelectedRow() >= 0);
}

int Hub::getNumRows() { return shown.size(); }

void Hub::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, shown.size()))
        return;
    const auto& file = shown.getReference (row);
    if (selected)
    {
        g.setColour (theme::panelLight.brighter (0.1f));
        g.fillRoundedRectangle (4.0f, 2.0f, (float) width - 8.0f, (float) height - 4.0f, 4.0f);
    }
    // Petite icône de document, nom en gras, date à droite.
    const auto icon = juce::Rectangle<float> (16.0f, (float) height * 0.5f - 10.0f, 15.0f, 20.0f);
    juce::Path doc;
    doc.startNewSubPath (icon.getX(), icon.getY());
    doc.lineTo (icon.getRight() - 5.0f, icon.getY());
    doc.lineTo (icon.getRight(), icon.getY() + 5.0f);
    doc.lineTo (icon.getRight(), icon.getBottom());
    doc.lineTo (icon.getX(), icon.getBottom());
    doc.closeSubPath();
    g.setColour (theme::text);
    g.fillPath (doc);

    g.setFont (theme::font (16.0f, juce::Font::bold));
    g.drawText (file.getFileNameWithoutExtension(), 44, 0, width - 260, height, juce::Justification::centredLeft, true);
    g.setColour (theme::textDim);
    g.setFont (theme::font (14.0f));
    g.drawText (file.getLastModificationTime().formatted ("%d/%m/%Y  %H:%M"), width - 210, 0, 190, height, juce::Justification::centredRight);
}

void Hub::openRow (int row)
{
    if (juce::isPositiveAndBelow (row, shown.size()) && onOpen)
        onOpen (shown[row]);
}

void Hub::listBoxItemDoubleClicked (int row, const juce::MouseEvent&) { openRow (row); }
void Hub::returnKeyPressed (int row) { openRow (row); }
void Hub::selectedRowsChanged (int row) { openButton.setEnabled (row >= 0); }

void Hub::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    g.setGradientFill (juce::ColourGradient (theme::panel.brighter (0.05f), 0.0f, 0.0f, theme::panel, 0.0f, 88.0f, false));
    g.fillRect (0, 0, getWidth(), 88);
    theme::woodCheek (g, { 0.0f, 0.0f, 10.0f, (float) getHeight() });
    theme::woodCheek (g, { (float) getWidth() - 10.0f, 0.0f, 10.0f, (float) getHeight() });

    g.setColour (theme::text);
    g.setFont (theme::font (40.0f).italicised());
    g.drawText ("Grid Play", 34, 14, 260, 46, juce::Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.0f, juce::Font::bold).withExtraKerningFactor (0.35f));
    g.drawText ("LOAD & PLAY", 37, 58, 200, 16, juce::Justification::centredLeft);
    theme::sectionTitle (g, "PILOTE AUDIO", { getWidth() - 580, 14, 200, 14 });
    theme::sectionTitle (g, "SORTIE", { getWidth() - 330, 14, 200, 14 });
    g.setColour (theme::rule);
    g.fillRect (24, 87, getWidth() - 48, 1);

    // Zones arrondies, comme les panneaux de Cubase.
    g.setColour (theme::panel);
    g.fillRoundedRectangle (juce::Rectangle<float> (252.0f, 104.0f, (float) getWidth() - 278.0f, 74.0f), 6.0f);
    g.fillRoundedRectangle (juce::Rectangle<float> (252.0f, 188.0f, (float) getWidth() - 278.0f, (float) getHeight() - 250.0f), 6.0f);
    theme::sectionTitle (g, juce::String::fromUTF8 ("RÉCENTS"), { 268, 196, 200, 18 });
    if (shown.isEmpty())
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (14.0f));
        g.drawFittedText (search.getText().isEmpty() ? juce::String::fromUTF8 ("Aucun projet récent : créez-en un, ou ouvrez un fichier .gridplay.")
                                                     : juce::String::fromUTF8 ("Aucun projet ne correspond à la recherche."),
                          268, 240, getWidth() - 320, 40, juce::Justification::topLeft, 2);
    }
}

void Hub::resized()
{
    const int w = getWidth();
    driver.setBounds (w - 580, 32, 240, 30);
    output.setBounds (w - 330, 32, 220, 30);
    audioButton.setBounds (w - 104, 32, 80, 30);

    auto left = juce::Rectangle<int> (26, 104, 210, getHeight() - 120);
    projectsButton.setBounds (left.removeFromTop (36));
    left.removeFromTop (16);
    for (auto* b : { &midiButton, &pluginsButton, &manualButton, &siteButton })
    {
        b->setBounds (left.removeFromTop (34));
        left.removeFromTop (8);
    }
    startup.setBounds (26, getHeight() - 46, 220, 24);

    createButton.setBounds (270, 122, 220, 38);
    search.setBounds (w - 340, 126, 300, 30);
    list.setBounds (260, 220, w - 294, getHeight() - 290);
    browseButton.setBounds (252, getHeight() - 50, 220, 32);
    openButton.setBounds (w - 226, getHeight() - 50, 200, 32);
}
} // namespace gridplay
