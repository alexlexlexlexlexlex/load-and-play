// Le Hub, comme celui de Cubase : au démarrage, les projets récents (avec leur date),
// créer un projet, ouvrir un fichier, et les réglages rapides (pilote et sortie audio,
// audio / MIDI, instruments et effets).
#pragma once

#include "Theme.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace gridplay
{
class Hub : public juce::Component, private juce::ListBoxModel, private juce::ChangeListener
{
public:
    Hub (juce::AudioDeviceManager& devices, juce::RecentlyOpenedFilesList& recent, juce::PropertiesFile& settings);
    ~Hub() override;

    std::function<void()> onCreate, onBrowse, onAudioSettings, onScanPlugins;
    std::function<void (const juce::File&)> onOpen;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int) override;
    void returnKeyPressed (int row) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void openRow (int row);
    void filter();
    void refreshDevices();

    juce::AudioDeviceManager& devices;
    juce::RecentlyOpenedFilesList& recent;
    juce::PropertiesFile& settings;
    juce::Array<juce::File> shown;

    juce::ComboBox driver, output;
    juce::TextButton audioButton, projectsButton, midiButton, pluginsButton, manualButton, siteButton;
    juce::TextButton createButton, browseButton, openButton;
    juce::ToggleButton startup;
    juce::TextEditor search;
    juce::ListBox list;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Hub)
};
} // namespace gridplay
