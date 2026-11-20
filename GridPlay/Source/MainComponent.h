// La fenêtre principale : barre de transport, vue session, menus et raccourcis,
// fichiers .gridplay, fenêtres des instruments et réglages audio / MIDI.
#pragma once

#include "ApcMini.h"
#include "SessionView.h"
#include "TransportControls.h"
#include "NoteEditor.h"
#include "ChannelEditor.h"
#include "Hub.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace gridplay
{
class MainComponent : public juce::Component,
                      public juce::ApplicationCommandTarget,
                      public juce::MenuBarModel,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void requestQuit (std::function<void()> quit);
    // Outil de développement : lance la scène 1, attend, enregistre une image de la fenêtre.
    void snapshot (const juce::File& png, std::function<void()> done);
    void openFile (const juce::File& file);
    void showHubIfWanted();                    // au démarrage, si l'option est cochée
    juce::String getWindowTitle() const;
    std::function<void()> onTitleChanged;

    void paint (juce::Graphics&) override;
    static constexpr int headerH = 60, cheek = 12;
    void resized() override;

    // Commandes
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info) override;
    bool perform (const InvocationInfo& info) override;

    // Menus
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int index, const juce::String& name) override;
    void menuItemSelected (int itemId, int menuIndex) override;

    juce::ApplicationCommandManager commands;

private:
    enum CommandIDs
    {
        newSong = 1, openSong, saveSong, saveSongAs,
        insertTrack, insertScene, deleteSelection, duplicate, copyClip, pasteClip, renameClip,
        launchSelection, playStop, stopAllClips, audioSettings, scanPlugins, editClip,
        undo, redo, deleteTrack, deleteScene, renameTrack, launchScene, armTrack, toggleDrumMap,
        importMidi, exportClip, toggleLink, channelSettings, masterSettings, toggleDetailed,
        openHub, closePluginWindows, showManual, showWebsite
    };
    // Éléments de menu qui ne sont pas des commandes (sous-menus).
    static constexpr int recentBase = 1000, clearRecent = 1999, quantizeBase = 2000, recordBase = 2100, quitItem = 2200;

    class PluginWindow;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    bool isDirty() const;
    void askToSave (std::function<void()> then);
    void save (std::function<void()> then);
    void saveAs (std::function<void()> then);
    bool writeTo (const juce::File& file);
    void openPluginWindow (int track);
    void openEditor (int track, int scene);
    void openChannel (int track);              // -1 : master
    void openInsertWindow (int target, int slot);
    void closeEditor();
    void closePluginWindow (juce::AudioPluginInstance* plugin);
    void showScanner();
    void showAudioSettings();
    void toggleWindow (std::unique_ptr<juce::DocumentWindow>& window, std::function<void()> open);
    void saveSettings();
    void updateTitle();
    void newProject (std::function<void()> then);
    void browseForProject();
    void rememberRecent (const juce::File& file);
    void showHub();
    void closeHub();
    void undoInActiveWindow (bool redo);
    ChannelEditor* activeChannelEditor() const;

    juce::ApplicationProperties properties;
    theme::LookAndFeel lookAndFeel;
    AudioEngine engine;
    std::unique_ptr<ApcMini> apc;

    TransportButton playButton { TransportButton::Icon::play }, stopButton { TransportButton::Icon::stop };
    TempoControl tempo;
    juce::TextButton linkButton { "Link" };
    juce::Label quantizeLabel, recordLabel;
    juce::ComboBox quantize, recordLength;
    juce::Label position, apcStatus;
    juce::TextButton pluginsButton { "Instruments" }, settingsButton { "Audio / MIDI" };
    SessionView session;
    juce::TooltipWindow tooltips { this, 600 };

    juce::File currentFile;
    int savedChangeCount = 0;
    juce::String lastTitle;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::OwnedArray<PluginWindow> pluginWindows;
    std::unique_ptr<juce::DocumentWindow> scannerWindow;
    std::unique_ptr<juce::DocumentWindow> editorWindow;
    juce::OwnedArray<juce::DocumentWindow> channelWindows;
    NoteEditor* editor = nullptr;
    std::unique_ptr<juce::DocumentWindow> settingsWindow;
    std::unique_ptr<juce::DocumentWindow> hubWindow;
    juce::RecentlyOpenedFilesList recentFiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
} // namespace gridplay
