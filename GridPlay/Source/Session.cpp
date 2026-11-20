#include "Session.h"

namespace gridplay
{
namespace
{
    constexpr double epsilon = 1.0e-9;

    juce::MidiMessage noteOff (int pitch) { return juce::MidiMessage::noteOff (1, pitch); }
}

Session::Session()
{
    song = makeDefaultSong();
    resizeRuntime();
}

void Session::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
}

void Session::resizeRuntime()
{
    runtime.resize (song.tracks.size());
    for (auto& t : song.tracks)
        t.slots.resize (song.scenes.size());
}

//==============================================================================
double Session::boundaryAfter (double from) const
{
    const double q = song.quantize;
    if (q <= 0.0)
        return from;
    return std::ceil (from / q - epsilon) * q;
}

void Session::queue (int track, Queued what, int slot)
{
    auto& rt = runtime[(size_t) track];
    rt.queued = what;
    rt.queuedSlot = slot;
    rt.queuedBeat = -1.0;
    if (! playing && what != Queued::stop)
        play();
    else if (! playing)
        rt.queued = Queued::none;
}

void Session::launchSlot (int track, int scene)
{
    if (! juce::isPositiveAndBelow (track, song.numTracks()) || ! juce::isPositiveAndBelow (scene, song.numScenes()))
        return;

    const auto& rt = runtime[(size_t) track];
    if (rt.recording && rt.recordSlot == scene)
        queue (track, Queued::play, scene);                 // fin de l'enregistrement : la boucle continue
    else if (song.clipAt (track, scene) != nullptr)
        queue (track, Queued::play, scene);
    else if (song.tracks[(size_t) track].arm)
        queue (track, Queued::record, scene);
    else
        queue (track, Queued::stop, -1);
}

void Session::stopTrack (int track)
{
    if (juce::isPositiveAndBelow (track, song.numTracks()))
        queue (track, Queued::stop, -1);
}

void Session::launchScene (int scene)
{
    if (! juce::isPositiveAndBelow (scene, song.numScenes()))
        return;
    if (! playing)
        play();
    for (int t = 0; t < song.numTracks(); ++t)
    {
        if (song.clipAt (t, scene) != nullptr)
            queue (t, Queued::play, scene);
        else if (song.tracks[(size_t) t].arm)
            queue (t, Queued::record, scene);
        else
            queue (t, Queued::stop, -1);
    }
}

void Session::stopAllClips()
{
    for (int t = 0; t < song.numTracks(); ++t)
        stopTrack (t);
}

void Session::play()
{
    if (playing)
        return;
    playing = true;
    beat = 0.0;
    for (auto& rt : runtime)
        rt.queuedBeat = -1.0;
}

void Session::stop()
{
    for (int t = 0; t < song.numTracks(); ++t)
    {
        auto& rt = runtime[(size_t) t];
        if (rt.recording)
            finishRecording (t, beat);
        rt.playingSlot = -1;
        rt.queued = Queued::none;
        rt.queuedSlot = -1;
    }
    playing = false;
    beat = 0.0;
    // Les notes des clips seront coupées par le prochain bloc.
    for (auto& rt : runtime)
        for (auto& a : rt.active)
            a.offBeat = -1.0;
}

//==============================================================================
SlotState Session::getSlotState (int track, int scene) const
{
    if (! juce::isPositiveAndBelow (track, song.numTracks()) || ! juce::isPositiveAndBelow (scene, song.numScenes()))
        return SlotState::empty;

    const auto& rt = runtime[(size_t) track];
    if (rt.recording && rt.recordSlot == scene)
        return SlotState::recording;
    if (rt.queued == Queued::record && rt.queuedSlot == scene)
        return SlotState::recordQueued;
    if (rt.queued == Queued::play && rt.queuedSlot == scene && rt.playingSlot != scene)
        return SlotState::playQueued;
    if (rt.playingSlot == scene)
        return SlotState::playing;
    return song.clipAt (track, scene) != nullptr ? SlotState::stopped : SlotState::empty;
}

int Session::getPlayingSlot (int track) const
{
    return juce::isPositiveAndBelow (track, (int) runtime.size()) ? runtime[(size_t) track].playingSlot : -1;
}

double Session::getClipProgress (int track) const
{
    if (! juce::isPositiveAndBelow (track, (int) runtime.size()) || ! playing)
        return 0.0;
    const auto& rt = runtime[(size_t) track];
    if (rt.recording)
    {
        const double span = song.recordBars > 0 ? song.recordBars * beatsPerBar : (double) beatsPerBar;
        return std::fmod (juce::jmax (0.0, beat - rt.recordStart), span) / span;
    }
    if (auto clip = song.clipAt (track, rt.playingSlot))
        return std::fmod (juce::jmax (0.0, beat - rt.clipStart), clip->length) / clip->length;
    return 0.0;
}

bool Session::isTrackActive (int track) const
{
    if (! juce::isPositiveAndBelow (track, (int) runtime.size()))
        return false;
    const auto& rt = runtime[(size_t) track];
    return rt.playingSlot >= 0 || rt.recording;
}

bool Session::isSceneActive (int scene) const
{
    for (int t = 0; t < song.numTracks(); ++t)
    {
        const auto state = getSlotState (t, scene);
        if (state == SlotState::playing || state == SlotState::recording)
            return true;
    }
    return false;
}

bool Session::anySolo() const
{
    for (const auto& t : song.tracks)
        if (t.solo)
            return true;
    return false;
}

//==============================================================================
void Session::setSong (Song newSong)
{
    song = std::move (newSong);
    runtime.clear();
    resizeRuntime();
    playing = false;
    beat = 0.0;
}

void Session::insertTrack (int index, Track track)
{
    index = juce::jlimit (0, song.numTracks(), index);
    track.slots.resize (song.scenes.size());
    song.tracks.insert (song.tracks.begin() + index, std::move (track));
    runtime.insert (runtime.begin() + index, Runtime());
}

void Session::removeTrack (int index)
{
    if (! juce::isPositiveAndBelow (index, song.numTracks()))
        return;
    song.tracks.erase (song.tracks.begin() + index);
    runtime.erase (runtime.begin() + index);
}

void Session::insertScene (int index, Scene scene)
{
    index = juce::jlimit (0, song.numScenes(), index);
    song.scenes.insert (song.scenes.begin() + index, std::move (scene));
    for (auto& t : song.tracks)
        t.slots.insert (t.slots.begin() + index, nullptr);
    for (auto& rt : runtime)
    {
        if (rt.playingSlot >= index) ++rt.playingSlot;
        if (rt.queuedSlot >= index) ++rt.queuedSlot;
        if (rt.recordSlot >= index) ++rt.recordSlot;
    }
}

void Session::removeScene (int index)
{
    if (! juce::isPositiveAndBelow (index, song.numScenes()))
        return;
    for (int t = 0; t < song.numTracks(); ++t)
    {
        auto& rt = runtime[(size_t) t];
        if (rt.playingSlot == index)  { rt.playingSlot = -1; for (auto& a : rt.active) a.offBeat = -1.0; }
        if (rt.queuedSlot == index)   { rt.queued = Queued::none; rt.queuedSlot = -1; }
        if (rt.recordSlot == index)   { rt.recording = false; rt.recordSlot = -1; rt.recordClip.reset(); }
        if (rt.playingSlot > index) --rt.playingSlot;
        if (rt.queuedSlot > index) --rt.queuedSlot;
        if (rt.recordSlot > index) --rt.recordSlot;
        auto& slots = song.tracks[(size_t) t].slots;
        slots.erase (slots.begin() + index);
    }
    song.scenes.erase (song.scenes.begin() + index);
}

void Session::setClip (int track, int scene, ClipPtr clip)
{
    if (! juce::isPositiveAndBelow (track, song.numTracks()) || ! juce::isPositiveAndBelow (scene, song.numScenes()))
        return;
    auto& rt = runtime[(size_t) track];
    if (rt.recording && rt.recordSlot == scene)
    {
        rt.recording = false;
        rt.recordSlot = -1;
        rt.recordClip.reset();
    }
    if (clip == nullptr)
    {
        if (rt.playingSlot == scene) { rt.playingSlot = -1; for (auto& a : rt.active) a.offBeat = -1.0; }
        if (rt.queuedSlot == scene && rt.queued == Queued::play) { rt.queued = Queued::none; rt.queuedSlot = -1; }
    }
    song.tracks[(size_t) track].slots[(size_t) scene] = std::move (clip);
}

void Session::moveClip (int fromTrack, int fromScene, int toTrack, int toScene, bool copy)
{
    auto clip = song.clipAt (fromTrack, fromScene);
    if (clip == nullptr || (fromTrack == toTrack && fromScene == toScene))
        return;
    if (! juce::isPositiveAndBelow (toTrack, song.numTracks()) || ! juce::isPositiveAndBelow (toScene, song.numScenes()))
        return;
    setClip (toTrack, toScene, copy ? clip->copy() : clip);
    if (! copy)
        setClip (fromTrack, fromScene, nullptr);
}

void Session::forgetActiveNotes (int track)
{
    if (! juce::isPositiveAndBelow (track, (int) runtime.size()))
        return;
    auto& rt = runtime[(size_t) track];
    rt.active.clear();
    rt.liveHeld.fill (false);
}

juce::MidiBuffer Session::takeAllNotesOff (int track)
{
    juce::MidiBuffer out;
    if (! juce::isPositiveAndBelow (track, (int) runtime.size()))
        return out;
    auto& rt = runtime[(size_t) track];
    for (const auto& a : rt.active)
        out.addEvent (noteOff (a.pitch), 0);
    for (int p = 0; p < 128; ++p)
        if (rt.liveHeld[(size_t) p])
            out.addEvent (noteOff (p), 0);
    out.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    forgetActiveNotes (track);
    return out;
}

//==============================================================================
void Session::startRecording (int track, int slot, double at)
{
    auto& rt = runtime[(size_t) track];
    const auto& tr = song.tracks[(size_t) track];
    auto clip = std::make_shared<Clip>();
    clip->name = tr.name;
    clip->colour = tr.colour;
    clip->length = song.recordBars > 0 ? song.recordBars * beatsPerBar : (double) beatsPerBar;
    clip->notes.reserve (4096);   // pas d'allocation pendant le jeu
    song.tracks[(size_t) track].slots[(size_t) slot] = clip;

    rt.recording = true;
    rt.recordSlot = slot;
    rt.recordStart = at;
    rt.recordClip = clip;
    rt.heldStart.fill (-1.0);
}

void Session::finishRecording (int track, double at)
{
    auto& rt = runtime[(size_t) track];
    if (! rt.recording)
        return;

    auto clip = rt.recordClip;
    const double elapsed = juce::jmax (0.0, at - rt.recordStart);
    for (int p = 0; p < 128; ++p)
        if (rt.heldStart[(size_t) p] >= 0.0)
        {
            clip->notes.push_back ({ rt.heldStart[(size_t) p], juce::jmax (1.0 / 64.0, elapsed - rt.heldStart[(size_t) p]), p,
                                     juce::jmax (1, (int) rt.heldVelocity[(size_t) p]) });
            rt.heldStart[(size_t) p] = -1.0;
        }

    double length = song.recordBars > 0 ? song.recordBars * beatsPerBar
                                        : juce::jmax (1.0, std::ceil (elapsed - 1.0e-6));
    clip->length = length;
    clip->notes.erase (std::remove_if (clip->notes.begin(), clip->notes.end(),
                                       [length] (const Note& n) { return n.start >= length; }),
                       clip->notes.end());
    std::sort (clip->notes.begin(), clip->notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });

    rt.recording = false;
    rt.playingSlot = rt.recordSlot;
    rt.clipStart = rt.recordStart;
    rt.recordSlot = -1;
    rt.recordClip.reset();
}

void Session::allNotesOff (Runtime& rt, int sample, juce::MidiBuffer& out)
{
    for (const auto& a : rt.active)
        out.addEvent (noteOff (a.pitch), sample);
    rt.active.clear();
}

void Session::applyQueued (int track, double at, int sample, juce::MidiBuffer& out)
{
    auto& rt = runtime[(size_t) track];
    const auto what = rt.queued;
    const int slot = rt.queuedSlot;
    rt.queued = Queued::none;
    rt.queuedSlot = -1;
    rt.queuedBeat = -1.0;

    if (rt.recording)
    {
        const bool keepLooping = what == Queued::play && slot == rt.recordSlot;
        finishRecording (track, at);
        if (keepLooping)
            return;
    }

    allNotesOff (rt, sample, out);
    rt.playingSlot = -1;

    if (what == Queued::play && song.clipAt (track, slot) != nullptr)
    {
        rt.playingSlot = slot;
        rt.clipStart = at;
    }
    else if (what == Queued::record && juce::isPositiveAndBelow (slot, song.numScenes()))
    {
        startRecording (track, slot, at);
    }
}

void Session::playRange (int track, double from, double to, double blockStart, double samplesPerBeat,
                         int numSamples, juce::MidiBuffer& out, bool muted)
{
    auto& rt = runtime[(size_t) track];
    auto sampleFor = [&] (double b)
    {
        return juce::jlimit (0, numSamples - 1, (int) std::floor ((b - blockStart) * samplesPerBeat + 1.0e-6));
    };
    auto emitOffs = [&]
    {
        for (size_t i = 0; i < rt.active.size();)
        {
            if (rt.active[i].offBeat < to - epsilon)
            {
                out.addEvent (noteOff (rt.active[i].pitch), sampleFor (juce::jmax (from, rt.active[i].offBeat)));
                rt.active.erase (rt.active.begin() + (std::ptrdiff_t) i);
            }
            else
                ++i;
        }
    };

    emitOffs();

    if (! muted && rt.playingSlot >= 0)
        if (auto clip = song.clipAt (track, rt.playingSlot))
        {
            const double length = clip->length;
            for (const auto& n : clip->notes)
            {
                if (n.start >= length)
                    continue;
                const double first = rt.clipStart + n.start;
                double k = from > first ? std::ceil ((from - first) / length - epsilon) : 0.0;
                for (double at = first + k * length; at < to - epsilon; at += length)
                {
                    if (at < from - epsilon)
                        continue;
                    const int sample = sampleFor (at);
                    for (size_t i = 0; i < rt.active.size();)   // même note encore tenue : on la coupe d'abord
                    {
                        if (rt.active[i].pitch == n.pitch)
                        {
                            out.addEvent (noteOff (n.pitch), sample);
                            rt.active.erase (rt.active.begin() + (std::ptrdiff_t) i);
                        }
                        else
                            ++i;
                    }
                    out.addEvent (juce::MidiMessage::noteOn (1, n.pitch, (juce::uint8) n.velocity), sample);
                    rt.active.push_back ({ n.pitch, at + juce::jlimit (1.0e-4, length, n.length) });
                }
            }
        }

    emitOffs();
}

void Session::process (int numSamples, const std::vector<LiveEvent>& live, std::vector<juce::MidiBuffer>& outputs)
{
    if (outputs.size() != song.tracks.size())
        outputs.resize (song.tracks.size());
    for (auto& o : outputs)
        o.clear();
    if (numSamples <= 0)
        return;

    const double samplesPerBeat = sampleRate * 60.0 / song.tempo;
    const double b0 = beat;
    const double b1 = playing ? b0 + numSamples / samplesPerBeat : b0;
    const bool solo = anySolo();

    for (int t = 0; t < song.numTracks(); ++t)
    {
        auto& rt = runtime[(size_t) t];
        const auto& tr = song.tracks[(size_t) t];
        auto& out = outputs[(size_t) t];
        const bool muted = tr.mute || (solo && ! tr.solo);

        // Notes à couper tout de suite (transport arrêté, clip effacé...).
        for (size_t i = 0; i < rt.active.size();)
        {
            if (rt.active[i].offBeat < 0.0)
            {
                out.addEvent (noteOff (rt.active[i].pitch), 0);
                rt.active.erase (rt.active.begin() + (std::ptrdiff_t) i);
            }
            else
                ++i;
        }

        if (muted && ! rt.wasMuted)
        {
            allNotesOff (rt, 0, out);
            for (int p = 0; p < 128; ++p)
                if (rt.liveHeld[(size_t) p])
                {
                    out.addEvent (noteOff (p), 0);
                    rt.liveHeld[(size_t) p] = false;
                }
        }
        rt.wasMuted = muted;

        if (playing)
        {
            if (rt.queued != Queued::none && rt.queuedBeat < 0.0)
                rt.queuedBeat = boundaryAfter (b0);

            double pos = b0;
            for (int guard = 0; guard < 8; ++guard)
            {
                double next = b1;
                bool queuedHere = false, recordEnd = false;
                if (rt.queued != Queued::none && rt.queuedBeat < b1 - epsilon)
                {
                    next = juce::jmax (pos, rt.queuedBeat);
                    queuedHere = true;
                }
                if (rt.recording && song.recordBars > 0)
                {
                    // Longueur fixe atteinte : la boucle enregistrée se met à jouer.
                    const double end = rt.recordStart + song.recordBars * beatsPerBar;
                    if (end < b1 - epsilon && (! queuedHere || end <= next + epsilon))
                    {
                        next = juce::jmax (pos, end);
                        recordEnd = true;
                        queuedHere = false;
                    }
                }

                playRange (t, pos, next, b0, samplesPerBeat, numSamples, out, muted);
                if (! queuedHere && ! recordEnd)
                    break;

                const int sample = juce::jlimit (0, numSamples - 1, (int) std::floor ((next - b0) * samplesPerBeat + 1.0e-6));
                if (recordEnd)
                    finishRecording (t, next);
                else
                    applyQueued (t, next, sample, out);
                pos = next;
            }
        }

        // Claviers : jeu en direct et enregistrement.
        for (const auto& ev : live)
        {
            if (ev.track != t)
                continue;
            const auto& m = ev.message;
            if (m.isNoteOn())
            {
                if (muted)
                    continue;
                const int p = m.getNoteNumber();
                if (rt.liveHeld[(size_t) p])
                    out.addEvent (noteOff (p), 0);
                out.addEvent (juce::MidiMessage::noteOn (1, p, m.getVelocity()), 0);
                rt.liveHeld[(size_t) p] = true;
                if (rt.recording)
                {
                    rt.heldStart[(size_t) p] = juce::jmax (0.0, b0 - rt.recordStart);
                    rt.heldVelocity[(size_t) p] = m.getVelocity();
                }
            }
            else if (m.isNoteOff())
            {
                const int p = m.getNoteNumber();
                if (rt.liveHeld[(size_t) p])
                {
                    out.addEvent (noteOff (p), 0);
                    rt.liveHeld[(size_t) p] = false;
                }
                if (rt.recording && rt.heldStart[(size_t) p] >= 0.0)
                {
                    const double start = rt.heldStart[(size_t) p];
                    const double end = juce::jmax (0.0, b0 - rt.recordStart);
                    rt.recordClip->notes.push_back ({ start, juce::jmax (1.0 / 64.0, end - start), p,
                                                      juce::jmax (1, (int) rt.heldVelocity[(size_t) p]) });
                    rt.heldStart[(size_t) p] = -1.0;
                }
            }
            else if (! muted && (m.isController() || m.isPitchWheel() || m.isChannelPressure() || m.isAftertouch()))
            {
                auto copy = m;
                copy.setChannel (1);
                out.addEvent (copy, 0);
            }
        }
    }

    beat = b1;
}
} // namespace gridplay
