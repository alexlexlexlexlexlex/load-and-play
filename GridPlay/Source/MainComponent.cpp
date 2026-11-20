#include "MainComponent.h"

namespace gridplay
{
namespace
{
    const double quantizeValues[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
    const char* quantizeNames[] = { "Aucune", "1/16", "1/8", "1/4", "1/2", "1 mesure", "2 mesures", "4 mesures" };
    const int recordValues[] = { 0, 1, 2, 4, 8 };
    const char* recordNames[] = { "Libre", "1 mesure", "2 mesures", "4 mesures", "8 mesures" };

    juce::PropertiesFile::Options settingsOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Grid Play";
        o.folderName = "Load & Play";
        o.filenameSuffix = "settings";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }
}

//==============================================================================
// La fenêtre d'un instrument virtuel.
class MainComponent::PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (juce::AudioPluginInstance& p, const juce::String& title, std::function<void (PluginWindow*)> onClose)
        : juce::DocumentWindow (title, theme::panel, juce::DocumentWindow::closeButton), plugin (p), closed (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        juce::AudioProcessorEditor* view = p.hasEditor() ? p.createEditorIfNeeded() : nullptr;
        if (view == nullptr)
            view = new juce::GenericAudioProcessorEditor (p);
        setContentOwned (view, true);
        setResizable (view->isResizable(), false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    ~PluginWindow() override { clearContentComponent(); }   // l'éditeur part avant l'instrument
    void closeButtonPressed() override { closed (this); }

    juce::AudioPluginInstance& plugin;

private:
    std::function<void (PluginWindow*)> closed;
};

//==============================================================================
MainComponent::MainComponent() : session (engine)
{
    properties.setStorageParameters (settingsOptions());
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    // Instruments déjà trouvés, carte son choisie la dernière fois.
    auto* settings = properties.getUserSettings();
    if (auto xml = settings->getXmlValue ("knownPlugins"))
        engine.knownPlugins.recreateFromXml (*xml);
    engine.knownPlugins.addChangeListener (this);
    auto audioState = settings->getXmlValue ("audioState");
    engine.initialise (audioState.get());

    engine.onPluginRemoving = [this] (juce::AudioPluginInstance* p) { closePluginWindow (p); };
    engine.onError = [] (const juce::String& message)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Grid Play", message);
    };

    apc = std::make_unique<ApcMini> (engine);
    apc->onPadPressed = [this] (int t, int s) { session.selectCell (t, s); };

    // --- Barre de transport ---
    playButton.setTooltip ("Lecture (barre d'espace)");
    stopButton.setTooltip (juce::String::fromUTF8 ("Stop : arrête le transport et tous les clips"));
    playButton.setColour (juce::TextButton::buttonOnColourId, theme::play);
    playButton.onClick = [this] { engine.perform ([] (Session& s) { s.play(); }); };
    stopButton.onClick = [this] { engine.perform ([] (Session& s) { s.stop(); }); };

    tempo.onChange = [this] (double v) { engine.edit ([v] (Session& s) { s.song.tempo = v; }); };

    quantizeLabel.setText ("LANCEMENT", juce::dontSendNotification);
    recordLabel.setText ("ENREGISTREMENT", juce::dontSendNotification);
    for (auto* l : { &quantizeLabel, &recordLabel })
    {
        l->setFont (theme::font (10.5f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, theme::textDim);
        l->setJustificationType (juce::Justification::centredRight);
    }
    for (int i = 0; i < 8; ++i)
        quantize.addItem (quantizeNames[i], i + 1);
    for (int i = 0; i < 5; ++i)
        recordLength.addItem (recordNames[i], i + 1);
    quantize.setTooltip (juce::String::fromUTF8 ("Les clips lancés attendent ce moment pour démarrer"));
    recordLength.setTooltip ("Longueur d'enregistrement : libre (on re-clique pour finir) ou fixe");
    quantize.onChange = [this]
    {
        const double q = quantizeValues[juce::jlimit (0, 7, quantize.getSelectedId() - 1)];
        engine.edit ([q] (Session& s) { s.song.quantize = q; });
    };
    recordLength.onChange = [this]
    {
        const int bars = recordValues[juce::jlimit (0, 4, recordLength.getSelectedId() - 1)];
        engine.edit ([bars] (Session& s) { s.song.recordBars = bars; });
    };

    position.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::bold)));
    position.setJustificationType (juce::Justification::centred);
    position.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0d0d0d));
    position.setColour (juce::Label::outlineColourId, theme::rule);
    position.setColour (juce::Label::textColourId, theme::accent);
    position.setTooltip ("Position : mesure . temps . double-croche");
    apcStatus.setFont (juce::Font (juce::FontOptions (12.0f)));
    apcStatus.setJustificationType (juce::Justification::centredRight);

    pluginsButton.setTooltip (juce::String::fromUTF8 ("Chercher les instruments virtuels installés (VST3, AU, LV2)"));
    settingsButton.setTooltip ("Carte son et appareils MIDI");
    // Un second clic referme la fenêtre.
    pluginsButton.onClick = [this] { toggleWindow (scannerWindow, [this] { showScanner(); }); };
    settingsButton.onClick = [this] { toggleWindow (settingsWindow, [this] { showAudioSettings(); }); };

    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &stopButton, &tempo, &quantizeLabel, &quantize,
                                                             &recordLabel, &recordLength, &position, &apcStatus,
                                                             &pluginsButton, &settingsButton, &session })
        addAndMakeVisible (c);

    session.onOpenPlugin = [this] (int t) { openPluginWindow (t); };
    session.onScanPlugins = [this] { showScanner(); };
    session.onEditClip = [this] (int t, int s) { openEditor (t, s); };

    // --- Commandes et raccourcis ---
    commands.registerAllCommandsForTarget (this);
    commands.setFirstCommandTarget (this);
    addKeyListener (commands.getKeyMappings());
    setWantsKeyboardFocus (false);

    setSize (1320, 780);
    startTimerHz (15);
    timerCallback();
    juce::MessageManager::callAsync ([this] { session.grabKeyboardFocus(); });
}

MainComponent::~MainComponent()
{
    stopTimer();
    saveSettings();
    engine.knownPlugins.removeChangeListener (this);
    pluginWindows.clear();
    closeEditor();
    scannerWindow.reset();
    settingsWindow.reset();
    apc.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void MainComponent::saveSettings()
{
    auto* settings = properties.getUserSettings();
    if (auto xml = engine.deviceManager.createStateXml())
        settings->setValue ("audioState", xml.get());
    if (auto xml = engine.knownPlugins.createXml())
        settings->setValue ("knownPlugins", xml.get());
    settings->saveIfNeeded();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    saveSettings();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // Comme une façade de plugin Load & Play : joues en bois, en-tête, filet.
    g.fillAll (theme::background);
    g.setGradientFill (juce::ColourGradient (theme::panel.brighter (0.05f), 0.0f, 0.0f, theme::panel, 0.0f, (float) headerH, false));
    g.fillRect (0, 0, getWidth(), headerH);
    theme::woodCheek (g, { 0.0f, 0.0f, (float) cheek, (float) getHeight() });
    theme::woodCheek (g, { (float) (getWidth() - cheek), 0.0f, (float) cheek, (float) getHeight() });

    g.setColour (theme::text);
    g.setFont (theme::font (27.0f).italicised());
    g.drawText ("Grid Play", cheek + 14, 6, 160, 30, juce::Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::font (10.5f, juce::Font::bold).withExtraKerningFactor (0.35f));
    g.drawText ("LOAD & PLAY", cheek + 16, 34, 160, 14, juce::Justification::centredLeft);

    g.setColour (theme::rule);
    g.fillRect (cheek + 10, headerH - 1, getWidth() - 2 * cheek - 20, 1);
}

void MainComponent::resized()
{
    auto bar = getLocalBounds().removeFromTop (headerH).withTrimmedLeft (cheek + 170).withTrimmedRight (cheek + 12).reduced (0, 12);
    playButton.setBounds (bar.removeFromLeft (36));
    bar.removeFromLeft (4);
    stopButton.setBounds (bar.removeFromLeft (36));
    bar.removeFromLeft (14);
    tempo.setBounds (bar.removeFromLeft (156));
    bar.removeFromLeft (10);
    position.setBounds (bar.removeFromLeft (86));
    bar.removeFromLeft (4);
    quantizeLabel.setBounds (bar.removeFromLeft (80));
    quantize.setBounds (bar.removeFromLeft (96));
    recordLabel.setBounds (bar.removeFromLeft (112));
    recordLength.setBounds (bar.removeFromLeft (96));
    settingsButton.setBounds (bar.removeFromRight (104));
    bar.removeFromRight (6);
    pluginsButton.setBounds (bar.removeFromRight (104));
    bar.removeFromRight (10);
    apcStatus.setBounds (bar);

    session.setBounds (getLocalBounds().withTrimmedTop (headerH).withTrimmedLeft (cheek).withTrimmedRight (cheek));
}

void MainComponent::timerCallback()
{
    const auto& v = session.getSnapshot();
    const double beat = v.beat;
    const int bar = (int) (beat / beatsPerBar) + 1;
    const int beatInBar = (int) std::fmod (beat, (double) beatsPerBar) + 1;
    const int sixteenth = (int) (std::fmod (beat, 1.0) * 4.0) + 1;
    position.setText (juce::String (bar) + ". " + juce::String (beatInBar) + ". " + juce::String (sixteenth), juce::dontSendNotification);
    playButton.setToggleState (v.playing, juce::dontSendNotification);

    struct SongSettings { double tempo, quantize; int recordBars; };
    const auto s = engine.read ([] (const Session& se) { return SongSettings { se.song.tempo, se.song.quantize, se.song.recordBars }; });
    tempo.setTempo (s.tempo);
    pluginsButton.setToggleState (scannerWindow != nullptr, juce::dontSendNotification);
    settingsButton.setToggleState (settingsWindow != nullptr, juce::dontSendNotification);
    for (int i = 0; i < 8; ++i)
        if (std::abs (quantizeValues[i] - s.quantize) < 1.0e-6 && quantize.getSelectedId() != i + 1)
            quantize.setSelectedId (i + 1, juce::dontSendNotification);
    for (int i = 0; i < 5; ++i)
        if (recordValues[i] == s.recordBars && recordLength.getSelectedId() != i + 1)
            recordLength.setSelectedId (i + 1, juce::dontSendNotification);

    const auto status = apc->getStatus();
    apcStatus.setText (status.isNotEmpty() ? juce::String::fromUTF8 ("● ") + status : juce::String (juce::String::fromUTF8 ("APC mini non branché")),
                       juce::dontSendNotification);
    apcStatus.setColour (juce::Label::textColourId, status.isNotEmpty() ? theme::play : theme::textDim);

    updateTitle();
}

//==============================================================================
// Fichiers
bool MainComponent::isDirty() const   { return engine.getChangeCount() != savedChangeCount; }

juce::String MainComponent::getWindowTitle() const
{
    return (currentFile.existsAsFile() ? currentFile.getFileNameWithoutExtension() : juce::String ("Sans titre"))
         + (isDirty() ? " *" : "") + juce::String::fromUTF8 (" — Grid Play");
}

void MainComponent::updateTitle()
{
    const auto title = getWindowTitle();
    if (title != lastTitle)
    {
        lastTitle = title;
        if (onTitleChanged)
            onTitleChanged();
    }
}

bool MainComponent::writeTo (const juce::File& file)
{
    engine.storePluginStates();
    auto xml = engine.read ([] (const Session& s) { return songToXml (s.song); });
    if (! xml->writeTo (file))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Grid Play",
                                                "Impossible d'enregistrer " + file.getFullPathName());
        return false;
    }
    currentFile = file;
    savedChangeCount = engine.getChangeCount();
    properties.getUserSettings()->setValue ("lastFile", file.getFullPathName());
    updateTitle();
    return true;
}

void MainComponent::save (std::function<void()> then)
{
    if (currentFile.existsAsFile())
    {
        if (writeTo (currentFile) && then)
            then();
    }
    else
        saveAs (std::move (then));
}

void MainComponent::saveAs (std::function<void()> then)
{
    auto start = currentFile.existsAsFile() ? currentFile
                                            : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Sans titre.gridplay");
    chooser = std::make_unique<juce::FileChooser> ("Enregistrer le morceau", start, "*.gridplay");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, then] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File())
                                  return;
                              if (writeTo (file.withFileExtension ("gridplay")) && then)
                                  then();
                          });
}

void MainComponent::askToSave (std::function<void()> then)
{
    if (! isDirty())
    {
        then();
        return;
    }
    juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                           .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                           .withTitle ("Grid Play")
                                           .withMessage ("Enregistrer les modifications du morceau ?")
                                           .withButton ("Enregistrer")
                                           .withButton ("Ne pas enregistrer")
                                           .withButton ("Annuler"),
                                       [this, then] (int result)
                                       {
                                           if (result == 0) save (then);
                                           else if (result == 1) then();
                                       });
}

void MainComponent::openFile (const juce::File& file)
{
    askToSave ([this, file]
    {
        auto xml = juce::parseXML (file);
        Song song;
        if (xml == nullptr || ! songFromXml (*xml, song))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Grid Play",
                                                    "Ce fichier n'est pas un morceau Grid Play : " + file.getFileName());
            return;
        }
        engine.setSong (std::move (song));
        currentFile = file;
        savedChangeCount = engine.getChangeCount();
        session.selectCell (0, 0);
        updateTitle();
    });
}

void MainComponent::snapshot (const juce::File& png, std::function<void()> done)
{
    // GRIDPLAY_TEST_SCAN=<dossier> : scanne ce dossier comme la fenêtre Instruments (sans rien enregistrer).
    const auto scanFolder = juce::SystemStats::getEnvironmentVariable ("GRIDPLAY_TEST_SCAN", {});
    if (scanFolder.isNotEmpty())
        for (auto* format : engine.formatManager.getFormats())
        {
            juce::KnownPluginList list;
            juce::PluginDirectoryScanner scanner (list, *format, juce::FileSearchPath (scanFolder), true, {}, false);
            juce::String name;
            while (scanner.scanNextFile (true, name)) {}
            std::cerr << format->getName().toRawUTF8() << " : " << list.getNumTypes() << " instruments trouvés" << std::endl;
        }

    // GRIDPLAY_TEST_PLUGIN=<chemin d'un .vst3> : charge cet instrument sur la piste 1.
    const auto pluginPath = juce::SystemStats::getEnvironmentVariable ("GRIDPLAY_TEST_PLUGIN", {});
    if (pluginPath.isNotEmpty())
    {
        juce::OwnedArray<juce::PluginDescription> found;
        for (auto* format : engine.formatManager.getFormats())
            if (format->fileMightContainThisPluginType (pluginPath))
                format->findAllTypesForFile (found, pluginPath);
        if (! found.isEmpty())
            engine.setTrackPlugin (0, *found[0]);
    }
    engine.perform ([] (Session& s) { s.launchScene (0); });

    // GRIDPLAY_TEST_CLIP=piste,scène : ouvre l'éditeur de notes sur ce clip et en fait aussi une image.
    const auto editTarget = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("GRIDPLAY_TEST_CLIP", {}), ",", {});
    if (editTarget.size() == 2)
        openEditor (editTarget[0].getIntValue(), editTarget[1].getIntValue());

    if (juce::SystemStats::getEnvironmentVariable ("GRIDPLAY_TEST_EDITOR", {}).isNotEmpty())
        juce::Timer::callAfterDelay (1200, [this] { openPluginWindow (0); });
    juce::Timer::callAfterDelay (2300, [this, png, done]
    {
        session.selectCell (1, 2);
        timerCallback();
        if (auto* plugin = engine.getPlugin (0); plugin != nullptr && juce::SystemStats::getEnvironmentVariable ("GRIDPLAY_TEST_PLUGIN", {}).isNotEmpty())
            std::cerr << "Instrument de la piste 1 : " << plugin->getName().toRawUTF8()
                      << ", crête de sortie " << engine.takeOutputPeak() << std::endl;
        if (editor != nullptr)
        {
            auto editorImage = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
            const auto file = png.getSiblingFile (png.getFileNameWithoutExtension() + "-editeur.png");
            file.deleteFile();
            juce::FileOutputStream editorOut (file);
            juce::PNGImageFormat().writeImageToStream (editorImage, editorOut);
        }
        auto image = createComponentSnapshot (getLocalBounds(), true, 2.0f);
        png.deleteFile();
        juce::FileOutputStream out (png);
        juce::PNGImageFormat().writeImageToStream (image, out);
        out.flush();
        done();
    });
}

void MainComponent::requestQuit (std::function<void()> quit)
{
    askToSave (std::move (quit));
}

//==============================================================================
// Fenêtres
void MainComponent::openPluginWindow (int track)
{
    auto* plugin = engine.getPlugin (track);
    if (plugin == nullptr)
        return;
    for (auto* w : pluginWindows)
        if (&w->plugin == plugin)
        {
            w->toFront (true);
            return;
        }
    const auto title = session.getSnapshot().tracks[(size_t) track].name + juce::String::fromUTF8 (" — ") + plugin->getName();
    pluginWindows.add (new PluginWindow (*plugin, title, [this] (PluginWindow* w) { pluginWindows.removeObject (w); }));
}

void MainComponent::openEditor (int track, int scene)
{
    auto clip = engine.read ([&] (const Session& s) { return s.song.clipAt (track, scene); });
    if (clip == nullptr)
        return;
    session.selectCell (track, scene);

    if (editorWindow != nullptr && editor != nullptr)
    {
        editor->setClip (clip);
        editorWindow->toFront (true);
        editor->grabKeyboardFocus();
        return;
    }

    struct EditorWindow : public juce::DocumentWindow
    {
        EditorWindow (MainComponent& o) : juce::DocumentWindow (juce::String::fromUTF8 ("Éditeur de notes"), theme::background, allButtons), owner (o) {}
        void closeButtonPressed() override { juce::MessageManager::callAsync ([&o = owner] { o.closeEditor(); }); }
        MainComponent& owner;
    };

    auto window = std::make_unique<EditorWindow> (*this);
    auto* content = new NoteEditor (engine, clip);
    content->onClipGone = [this] { juce::MessageManager::callAsync ([this] { closeEditor(); }); };
    content->onCloseRequested = [this] { juce::MessageManager::callAsync ([this] { closeEditor(); }); };
    content->setSize (1180, 680);
    window->setContentOwned (content, true);
    window->setUsingNativeTitleBar (true);
    window->setResizable (true, false);
    window->setResizeLimits (760, 440, 10000, 10000);
    window->centreWithSize (window->getWidth(), window->getHeight());
    window->setVisible (true);
    window->addKeyListener (commands.getKeyMappings());   // Cmd S, Cmd O... marchent aussi depuis l'éditeur
    editor = content;
    editorWindow = std::move (window);
    editor->grabKeyboardFocus();
}

void MainComponent::closeEditor()
{
    editor = nullptr;
    editorWindow.reset();
}

void MainComponent::closePluginWindow (juce::AudioPluginInstance* plugin)
{
    for (int i = pluginWindows.size(); --i >= 0;)
        if (&pluginWindows[i]->plugin == plugin)
            pluginWindows.remove (i);
}

void MainComponent::showScanner()
{
    if (scannerWindow != nullptr)
    {
        scannerWindow->toFront (true);
        return;
    }

    struct ScannerWindow : public juce::DocumentWindow
    {
        ScannerWindow (MainComponent& o) : juce::DocumentWindow ("Instruments virtuels", theme::panel, closeButton), owner (o)
        {
            auto* settings = owner.properties.getUserSettings();
            // Fichier témoin : si un instrument plante pendant la recherche, il sera écarté la fois suivante.
            const auto pedal = settings->getFile().getSiblingFile ("Grid Play plugin scan.txt");
            auto* list = new juce::PluginListComponent (owner.engine.formatManager, owner.engine.knownPlugins, pedal, settings, true);
            list->setSize (720, 460);
            setContentOwned (list, true);
            setUsingNativeTitleBar (true);
            setResizable (true, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { owner.saveSettings(); juce::MessageManager::callAsync ([&o = owner] { o.scannerWindow.reset(); }); }
        MainComponent& owner;
    };

    scannerWindow = std::make_unique<ScannerWindow> (*this);
    if (engine.knownPlugins.getNumTypes() == 0)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Instruments virtuels",
                                                juce::String::fromUTF8 ("Cliquez sur « Options... » en bas de la fenêtre, puis « Scan for new or updated VST3 plug-ins »"
                                                                        " (et AudioUnit sur Mac) : Grid Play cherche les instruments installés."));
}

void MainComponent::showAudioSettings()
{
    if (settingsWindow != nullptr)
    {
        settingsWindow->toFront (true);
        return;
    }

    struct SettingsWindow : public juce::DocumentWindow
    {
        SettingsWindow (MainComponent& o) : juce::DocumentWindow ("Audio / MIDI", theme::panel, closeButton), owner (o)
        {
            auto* selector = new juce::AudioDeviceSelectorComponent (owner.engine.deviceManager, 0, 0, 2, 2, false, false, true, false);
            selector->setSize (520, 360);
            setContentOwned (selector, true);
            setUsingNativeTitleBar (true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { owner.saveSettings(); juce::MessageManager::callAsync ([&o = owner] { o.settingsWindow.reset(); }); }
        MainComponent& owner;
    };
    settingsWindow = std::make_unique<SettingsWindow> (*this);
}

void MainComponent::toggleWindow (std::unique_ptr<juce::DocumentWindow>& window, std::function<void()> open)
{
    if (window != nullptr)
    {
        saveSettings();
        window.reset();
    }
    else
        open();
}

//==============================================================================
// Commandes, raccourcis (proches de ceux d'Ableton Live) et menus
void MainComponent::getAllCommands (juce::Array<juce::CommandID>& c)
{
    c.addArray ({ newSong, openSong, saveSong, saveSongAs, insertTrack, insertScene, deleteSelection, duplicate,
                  copyClip, pasteClip, renameClip, launchSelection, playStop, stopAllClips, audioSettings, scanPlugins, editClip });
}

void MainComponent::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    const auto cmd = juce::ModifierKeys::commandModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;
    switch (id)
    {
        case newSong:         info.setInfo ("Nouveau morceau", {}, "Fichier", 0); info.addDefaultKeypress ('n', cmd); break;
        case openSong:        info.setInfo ("Ouvrir...", {}, "Fichier", 0); info.addDefaultKeypress ('o', cmd); break;
        case saveSong:        info.setInfo ("Enregistrer", {}, "Fichier", 0); info.addDefaultKeypress ('s', cmd); break;
        case saveSongAs:      info.setInfo ("Enregistrer sous...", {}, "Fichier", 0); info.addDefaultKeypress ('s', cmd | shift); break;
        case insertTrack:     info.setInfo (juce::String::fromUTF8 ("Insérer une piste MIDI"), {}, "Edition", 0); info.addDefaultKeypress ('t', cmd | shift); break;
        case insertScene:     info.setInfo (juce::String::fromUTF8 ("Insérer une scène"), {}, "Edition", 0); info.addDefaultKeypress ('i', cmd); break;
        case deleteSelection: info.setInfo ("Supprimer le clip", {}, "Edition", 0); info.setActive (session.hasClipSelected()); break;
        case duplicate:       info.setInfo ("Dupliquer le clip", {}, "Edition", 0); info.addDefaultKeypress ('d', cmd); info.setActive (session.hasClipSelected()); break;
        case copyClip:        info.setInfo ("Copier le clip", {}, "Edition", 0); info.addDefaultKeypress ('c', cmd); info.setActive (session.hasClipSelected()); break;
        case pasteClip:       info.setInfo ("Coller le clip", {}, "Edition", 0); info.addDefaultKeypress ('v', cmd); break;
        case renameClip:      info.setInfo ("Renommer le clip", {}, "Edition", 0); info.addDefaultKeypress ('r', cmd); info.setActive (session.hasClipSelected()); break;
        case launchSelection: info.setInfo (juce::String::fromUTF8 ("Lancer la case sélectionnée"), {}, "Transport", 0); break;
        case playStop:        info.setInfo ("Lecture / Stop", {}, "Transport", 0); info.addDefaultKeypress (juce::KeyPress::spaceKey, 0); break;
        case stopAllClips:    info.setInfo (juce::String::fromUTF8 ("Arrêter tous les clips"), {}, "Transport", 0); break;
        case audioSettings:   info.setInfo ("Audio / MIDI...", {}, "Options", 0); info.addDefaultKeypress (',', cmd); break;
        case editClip:        info.setInfo (juce::String::fromUTF8 ("Éditer les notes du clip"), {}, "Edition", 0); info.addDefaultKeypress ('e', cmd); info.setActive (session.hasClipSelected()); break;
        case scanPlugins:     info.setInfo ("Instruments virtuels...", {}, "Options", 0); break;
        default: break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case newSong:
            askToSave ([this]
            {
                engine.setSong (makeDefaultSong());
                currentFile = juce::File();
                savedChangeCount = engine.getChangeCount();
                session.selectCell (0, 0);
            });
            break;
        case openSong:
            chooser = std::make_unique<juce::FileChooser> ("Ouvrir un morceau", currentFile.getParentDirectory(), "*.gridplay");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) openFile (fc.getResult()); });
            break;
        case saveSong:        save ({}); break;
        case saveSongAs:      saveAs ({}); break;
        case insertTrack:     session.insertTrack(); break;
        case insertScene:     session.insertScene(); break;
        case deleteSelection: session.deleteSelected(); break;
        case duplicate:       session.duplicateSelected(); break;
        case copyClip:        session.copySelected(); break;
        case pasteClip:       session.pasteSelected(); break;
        case renameClip:      session.renameSelected(); break;
        case launchSelection: session.launchSelected(); break;
        case playStop:
            engine.perform ([] (Session& s) { if (s.isPlaying()) s.stop(); else s.play(); });
            break;
        case stopAllClips:    engine.perform ([] (Session& s) { s.stopAllClips(); }); break;
        case audioSettings:   showAudioSettings(); break;
        case scanPlugins:     showScanner(); break;
        case editClip:        session.editSelected(); break;
        default: return false;
    }
    return true;
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "Fichier", juce::String::fromUTF8 ("Édition"), "Transport", "Options" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu m;
    switch (index)
    {
        case 0:
            m.addCommandItem (&commands, newSong);
            m.addCommandItem (&commands, openSong);
            m.addSeparator();
            m.addCommandItem (&commands, saveSong);
            m.addCommandItem (&commands, saveSongAs);
            break;
        case 1:
            m.addCommandItem (&commands, insertTrack);
            m.addCommandItem (&commands, insertScene);
            m.addSeparator();
            m.addCommandItem (&commands, editClip);
            m.addCommandItem (&commands, renameClip);
            m.addCommandItem (&commands, duplicate);
            m.addCommandItem (&commands, copyClip);
            m.addCommandItem (&commands, pasteClip);
            m.addCommandItem (&commands, deleteSelection);
            break;
        case 2:
            m.addCommandItem (&commands, playStop);
            m.addCommandItem (&commands, launchSelection);
            m.addCommandItem (&commands, stopAllClips);
            break;
        case 3:
            m.addCommandItem (&commands, audioSettings);
            m.addCommandItem (&commands, scanPlugins);
            break;
        default:
            break;
    }
    return m;
}
} // namespace gridplay
