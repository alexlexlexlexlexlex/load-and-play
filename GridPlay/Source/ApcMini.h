// L'Akai APC mini (mk1 ou mk2), branché et reconnu tout seul :
// - pads : lancent les clips (la grille 8 x 8 en haut à gauche de Grid Play) ;
// - boutons du bas : arrêtent la piste (avec SHIFT : armer pour l'enregistrement) ;
// - boutons de droite : lancent la scène (avec SHIFT : tout arrêter) ;
// - faders : volume des pistes, le dernier pour le master.
// Les LED suivent l'état des cases : clip, en lecture, en attente, en enregistrement.
#pragma once

#include "AudioEngine.h"
#include "ApcProtocol.h"

namespace gridplay
{
class ApcMini : private juce::Timer
{
public:
    explicit ApcMini (AudioEngine& engine);
    ~ApcMini() override;

    juce::String getStatus() const;            // « APC mini mk2 », ou vide si absent
    std::function<void()> onStatusChanged;
    std::function<void (int track, int scene)> onPadPressed;   // pour sélectionner la case à l'écran

private:
    void timerCallback() override;
    void scanDevices();
    void connect (apc::Model model, const juce::MidiDeviceInfo& input, const juce::MidiDeviceInfo& output);
    void disconnect();
    void handle (const juce::MidiMessage& message);
    void updateLeds();
    void send (const juce::MidiMessage& message);

    AudioEngine& engine;
    apc::Model model = apc::Model::none;
    juce::String inputId, outputId, deviceName;
    std::unique_ptr<juce::MidiOutput> output;
    std::array<int, 128> sent {};     // dernier état envoyé à chaque LED (-1 = inconnu)
    bool shift = false;
    int ticks = 0;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};
} // namespace gridplay
