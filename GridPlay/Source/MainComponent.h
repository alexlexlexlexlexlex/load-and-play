// La fenêtre principale : barre de transport, vue session, menus et raccourcis,
// fichiers .gridplay, fenêtres des instruments et réglages audio / MIDI.
#pragma once

#include "ApcMini.h"
#include "SessionView.h"
#include "TransportControls.h"

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
    juce::String getWindowTitle() const;
    std::function<void()> onTitleChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Commandes
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info) override;
    bool perform (const InvocationInfo& info) override;

    // Menus
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int index, const juce::String& name) override;
    void menuItemSelected (int, int) override {}

    juce::ApplicationCommandManager commands;

private:
    enum CommandIDs
    {
        newSong = 1, openSong, saveSong, saveSongAs,
        insertTrack, insertScene, deleteSelection, duplicate, copyClip, pasteClip, renameClip,
        launchSelection, playStop, stopAllClips, audioSettings, scanPlugins
    };

    class PluginWindow;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    bool isDirty() const;
    void askToSave (std::function<void()> then);
    void save (std::function<void()> then);
    void saveAs (std::function<void()> then);
    bool writeTo (const juce::File& file);
    void openPluginWindow (int track);
    void closePluginWindow (juce::AudioPluginInstance* plugin);
    void showScanner();
    void showAudioSettings();
    void toggleWindow (std::unique_ptr<juce::DocumentWindow>& window, std::function<void()> open);
    void saveSettings();
    void updateTitle();

    juce::ApplicationProperties properties;
    theme::LookAndFeel lookAndFeel;
    AudioEngine engine;
    std::unique_ptr<ApcMini> apc;

    TransportButton playButton { TransportButton::Icon::play }, stopButton { TransportButton::Icon::stop };
    TempoControl tempo;
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
    std::unique_ptr<juce::DocumentWindow> settingsWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
} // namespace gridplay
