#include "ApcMini.h"

namespace gridplay
{
ApcMini::ApcMini (AudioEngine& e) : engine (e)
{
    sent.fill (-1);
    scanDevices();
    startTimerHz (30);
}

ApcMini::~ApcMini()
{
    *alive = false;
    stopTimer();
    disconnect();
}

juce::String ApcMini::getStatus() const
{
    if (model == apc::Model::none)
        return {};
    return model == apc::Model::mk2 ? "APC mini mk2" : "APC mini";
}

//==============================================================================
void ApcMini::scanDevices()
{
    auto best = [] (const juce::Array<juce::MidiDeviceInfo>& devices)
    {
        juce::MidiDeviceInfo chosen;
        int score = -1;
        for (const auto& d : devices)
            if (const int s = apc::portScore (d.name); s > score)
            {
                score = s;
                chosen = d;
            }
        return chosen;
    };

    const auto input = best (juce::MidiInput::getAvailableDevices());
    const auto outputDevice = best (juce::MidiOutput::getAvailableDevices());
    const auto found = apc::modelFromName (input.name);

    if (found == apc::Model::none || outputDevice.identifier.isEmpty())
    {
        if (model != apc::Model::none)
            disconnect();
        return;
    }
    if (input.identifier != inputId || outputDevice.identifier != outputId)
        connect (found, input, outputDevice);
}

void ApcMini::connect (apc::Model newModel, const juce::MidiDeviceInfo& input, const juce::MidiDeviceInfo& out)
{
    disconnect();
    output = juce::MidiOutput::openDevice (out.identifier);
    if (output == nullptr)
        return;

    model = newModel;
    inputId = input.identifier;
    outputId = out.identifier;
    deviceName = input.name;
    sent.fill (-1);
    shift = false;

    auto safe = alive;
    engine.setController (inputId, [this, safe] (const juce::MidiMessage& m)
    {
        juce::MessageManager::callAsync ([this, safe, m] { if (*safe) handle (m); });
    });
    updateLeds();
    if (onStatusChanged)
        onStatusChanged();
}

void ApcMini::disconnect()
{
    if (output != nullptr)
    {
        // Éteint tout en partant.
        for (int note = 0; note < 128; ++note)
            if (sent[(size_t) note] > 0)
                output->sendMessageNow (juce::MidiMessage::noteOn (1, note, (juce::uint8) 0));
    }
    engine.setController ({}, nullptr);
    output.reset();
    const bool wasConnected = model != apc::Model::none;
    model = apc::Model::none;
    inputId = outputId = deviceName = {};
    if (wasConnected && onStatusChanged)
        onStatusChanged();
}

//==============================================================================
void ApcMini::handle (const juce::MidiMessage& message)
{
    const auto in = apc::parse (model, message);
    using Kind = apc::Input::Kind;

    switch (in.kind)
    {
        case Kind::shift:
            shift = in.pressed;
            break;

        case Kind::pad:
            if (in.pressed)
            {
                engine.perform ([&] (Session& s) { s.launchSlot (in.column, in.row); });
                if (onPadPressed)
                    onPadPressed (in.column, in.row);
            }
            break;

        case Kind::trackButton:
            if (! in.pressed)
                break;
            if (shift)
                engine.edit ([&] (Session& s)
                {
                    if (juce::isPositiveAndBelow (in.index, s.song.numTracks()))
                        s.song.tracks[(size_t) in.index].arm = ! s.song.tracks[(size_t) in.index].arm;
                });
            else
                engine.perform ([&] (Session& s) { s.stopTrack (in.index); });
            break;

        case Kind::sceneButton:
            if (! in.pressed)
                break;
            if (shift)
                engine.perform ([] (Session& s) { s.stopAllClips(); });
            else
                engine.perform ([&] (Session& s) { s.launchScene (in.index); });
            break;

        case Kind::fader:
            engine.edit ([&] (Session& s)
            {
                if (in.index == apc::numFaders - 1)
                    s.song.masterVolume = in.value;
                else if (juce::isPositiveAndBelow (in.index, s.song.numTracks()))
                    s.song.tracks[(size_t) in.index].volume = in.value;
            });
            break;

        case Kind::none:
            break;
    }
}

void ApcMini::send (const juce::MidiMessage& message)
{
    // Mémorise canal et vélocité pour n'envoyer que ce qui change.
    const int note = message.getNoteNumber();
    const int code = message.getChannel() * 256 + message.getVelocity();
    if (sent[(size_t) note] == code)
        return;
    sent[(size_t) note] = code;
    output->sendMessageNow (message);
}

void ApcMini::updateLeds()
{
    if (output == nullptr)
        return;

    struct PadInfo { apc::Pad pad = apc::Pad::off; juce::Colour colour; };
    std::array<PadInfo, 64> pads;
    std::array<int, 8> trackButtons {}, sceneButtons {};
    bool blinkOn = false;

    engine.read ([&] (const Session& s)
    {
        const auto& song = s.song;
        blinkOn = s.isPlaying() ? std::fmod (s.getBeat(), 1.0) < 0.5
                                : (juce::Time::getMillisecondCounter() / 250) % 2 == 0;
        for (int row = 0; row < apc::gridSize; ++row)
            for (int col = 0; col < apc::gridSize; ++col)
            {
                auto& info = pads[(size_t) (row * apc::gridSize + col)];
                if (col >= song.numTracks() || row >= song.numScenes())
                    continue;
                switch (s.getSlotState (col, row))
                {
                    case SlotState::empty:        info.pad = song.tracks[(size_t) col].arm ? apc::Pad::armed : apc::Pad::off; break;
                    case SlotState::stopped:      info.pad = apc::Pad::clip; break;
                    case SlotState::playQueued:   info.pad = apc::Pad::playQueued; break;
                    case SlotState::playing:      info.pad = apc::Pad::playing; break;
                    case SlotState::recordQueued: info.pad = apc::Pad::recordQueued; break;
                    case SlotState::recording:    info.pad = apc::Pad::recording; break;
                }
                if (auto clip = song.clipAt (col, row))
                    info.colour = clip->colour;
            }
        for (int i = 0; i < apc::gridSize; ++i)
        {
            if (shift)
                trackButtons[(size_t) i] = i < song.numTracks() && song.tracks[(size_t) i].arm ? 1 : 0;
            else
                trackButtons[(size_t) i] = s.isTrackActive (i) ? 1 : 0;
            sceneButtons[(size_t) i] = s.isSceneActive (i) ? 1 : 0;
        }
    });

    for (int row = 0; row < apc::gridSize; ++row)
        for (int col = 0; col < apc::gridSize; ++col)
        {
            const auto& info = pads[(size_t) (row * apc::gridSize + col)];
            send (apc::padMessage (model, apc::gridNote (row, col), info.pad, info.colour, blinkOn));
        }
    for (int i = 0; i < apc::gridSize; ++i)
    {
        send (apc::buttonMessage (apc::trackButtonNote (model, i), trackButtons[(size_t) i]));
        send (apc::buttonMessage (apc::sceneButtonNote (model, i), sceneButtons[(size_t) i]));
    }
}

void ApcMini::timerCallback()
{
    if (++ticks % 30 == 0)
        scanDevices();     // branché, débranché : une fois par seconde
    updateLeds();
}
} // namespace gridplay
