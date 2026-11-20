// Le morceau : pistes, scènes et clips MIDI, avec la sauvegarde en XML et
// l'import / export de fichiers MIDI standard.
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace gridplay
{
constexpr int beatsPerBar = 4;     // 4/4 pour l'instant
constexpr int defaultTracks = 8;
constexpr int defaultScenes = 8;

//==============================================================================
// Couleurs des clips et des pistes, dans l'esprit de la palette d'Ableton.
inline const std::vector<juce::Colour>& palette()
{
    static const std::vector<juce::Colour> colours {
        juce::Colour (0xffff94a6), juce::Colour (0xffffa529), juce::Colour (0xffcc9927), juce::Colour (0xfff7f47c),
        juce::Colour (0xffbffb00), juce::Colour (0xff1aff2f), juce::Colour (0xff25ffa8), juce::Colour (0xff5cffe8),
        juce::Colour (0xff8bc5ff), juce::Colour (0xff5480e4), juce::Colour (0xff92a7ff), juce::Colour (0xffd86ce4),
        juce::Colour (0xffe553a0), juce::Colour (0xffffffff), juce::Colour (0xffff3636), juce::Colour (0xfff66c03),
    };
    return colours;
}

//==============================================================================
struct Note
{
    double start = 0.0;    // en temps (noires) depuis le début du clip
    double length = 0.25;
    int pitch = 60;
    int velocity = 100;
    bool muted = false;    // note en sourdine : affichée, pas jouée
};

struct Clip
{
    juce::String name;
    juce::Colour colour = palette()[0];
    double length = beatsPerBar;   // longueur de la boucle, en temps
    std::vector<Note> notes;

    std::shared_ptr<Clip> copy() const { return std::make_shared<Clip> (*this); }
};

using ClipPtr = std::shared_ptr<Clip>;

//==============================================================================
enum class OutputType { none, plugin, midiPort };

struct Track
{
    // Identifiant pour retrouver la piste après un déplacement (non sauvegardé).
    int id = nextId();
    static int nextId() { static int counter = 0; return ++counter; }

    juce::String name;
    juce::Colour colour = palette()[0];

    // Entrée : un appareil MIDI par son nom (vide = tous les claviers), un canal (0 = tous).
    juce::String inputDevice;
    bool inputEnabled = true;
    int inputChannel = 0;

    // Sortie : un instrument virtuel chargé dans Grid Play, ou un port MIDI.
    OutputType outputType = OutputType::none;
    juce::String outputPort;              // nom du port MIDI
    int outputChannel = 1;                // canal MIDI de sortie (1 à 16)
    juce::String pluginDescription;       // description de l'instrument (XML)
    juce::String pluginName;
    juce::MemoryBlock pluginState;        // état de l'instrument, mis à jour à la sauvegarde

    float volume = 0.85f;                 // 0..1, 0,85 = 0 dB
    bool drumMap = false;                 // l'éditeur montre les sons de batterie (drum map GM)
    bool mute = false, solo = false, arm = false;

    std::vector<ClipPtr> slots;           // une case par scène, nullptr = vide
};

struct Scene
{
    juce::String name;
};

struct Song
{
    double tempo = 120.0;
    double quantize = beatsPerBar;   // lancement calé sur : 0 = aucun, sinon en temps
    int recordBars = 0;              // longueur d'enregistrement fixe (0 = libre)
    float masterVolume = 0.85f;
    std::vector<Track> tracks;
    std::vector<Scene> scenes;

    int numScenes() const { return (int) scenes.size(); }
    int numTracks() const { return (int) tracks.size(); }

    ClipPtr clipAt (int track, int scene) const
    {
        if (juce::isPositiveAndBelow (track, numTracks()) && juce::isPositiveAndBelow (scene, numScenes()))
            return tracks[(size_t) track].slots[(size_t) scene];
        return {};
    }
};

inline float volumeToGain (float volume)       { return volume * volume * (1.0f / (0.85f * 0.85f)); }
inline float volumeToDecibels (float volume)   { return juce::Decibels::gainToDecibels (volumeToGain (volume), -70.0f); }

inline Track makeTrack (int index, int numScenes)
{
    Track t;
    t.name = "MIDI " + juce::String (index + 1);
    t.colour = palette()[(size_t) ((index * 5) % (int) palette().size())];
    t.slots.resize ((size_t) numScenes);
    return t;
}

inline Song makeDefaultSong()
{
    Song song;
    for (int s = 0; s < defaultScenes; ++s)
        song.scenes.push_back ({ juce::String (s + 1) });
    for (int t = 0; t < defaultTracks; ++t)
        song.tracks.push_back (makeTrack (t, defaultScenes));
    return song;
}

//==============================================================================
// Sauvegarde : un fichier .gridplay est un XML lisible.
inline std::unique_ptr<juce::XmlElement> clipToXml (const Clip& clip)
{
    auto xml = std::make_unique<juce::XmlElement> ("Clip");
    xml->setAttribute ("name", clip.name);
    xml->setAttribute ("colour", clip.colour.toString());
    xml->setAttribute ("length", clip.length);
    for (const auto& n : clip.notes)
    {
        auto* e = xml->createNewChildElement ("N");
        e->setAttribute ("s", n.start);
        e->setAttribute ("l", n.length);
        e->setAttribute ("p", n.pitch);
        e->setAttribute ("v", n.velocity);
        if (n.muted)
            e->setAttribute ("m", true);
    }
    return xml;
}

inline ClipPtr clipFromXml (const juce::XmlElement& xml)
{
    auto clip = std::make_shared<Clip>();
    clip->name = xml.getStringAttribute ("name");
    clip->colour = juce::Colour::fromString (xml.getStringAttribute ("colour", palette()[0].toString()));
    clip->length = juce::jmax (0.25, xml.getDoubleAttribute ("length", beatsPerBar));
    for (auto* e : xml.getChildWithTagNameIterator ("N"))
        clip->notes.push_back ({ e->getDoubleAttribute ("s"), e->getDoubleAttribute ("l", 0.25),
                                 juce::jlimit (0, 127, e->getIntAttribute ("p", 60)),
                                 juce::jlimit (1, 127, e->getIntAttribute ("v", 100)), e->getBoolAttribute ("m") });
    return clip;
}

inline std::unique_ptr<juce::XmlElement> songToXml (const Song& song)
{
    auto xml = std::make_unique<juce::XmlElement> ("GridPlay");
    xml->setAttribute ("version", 1);
    xml->setAttribute ("tempo", song.tempo);
    xml->setAttribute ("quantize", song.quantize);
    xml->setAttribute ("recordBars", song.recordBars);
    xml->setAttribute ("master", (double) song.masterVolume);

    auto* scenes = xml->createNewChildElement ("Scenes");
    for (const auto& s : song.scenes)
        scenes->createNewChildElement ("Scene")->setAttribute ("name", s.name);

    auto* tracks = xml->createNewChildElement ("Tracks");
    for (const auto& t : song.tracks)
    {
        auto* e = tracks->createNewChildElement ("Track");
        e->setAttribute ("name", t.name);
        e->setAttribute ("colour", t.colour.toString());
        e->setAttribute ("input", t.inputDevice);
        e->setAttribute ("inputEnabled", t.inputEnabled);
        e->setAttribute ("inputChannel", t.inputChannel);
        e->setAttribute ("output", t.outputType == OutputType::plugin ? "plugin" : t.outputType == OutputType::midiPort ? "midi" : "none");
        e->setAttribute ("port", t.outputPort);
        e->setAttribute ("outputChannel", t.outputChannel);
        e->setAttribute ("volume", (double) t.volume);
        e->setAttribute ("mute", t.mute);
        e->setAttribute ("solo", t.solo);
        e->setAttribute ("arm", t.arm);
        e->setAttribute ("drumMap", t.drumMap);
        if (t.outputType == OutputType::plugin)
        {
            auto* p = e->createNewChildElement ("Plugin");
            p->setAttribute ("name", t.pluginName);
            p->setAttribute ("state", t.pluginState.toBase64Encoding());
            if (auto desc = juce::parseXML (t.pluginDescription))
                p->addChildElement (desc.release());
        }
        for (size_t s = 0; s < t.slots.size(); ++s)
            if (t.slots[s] != nullptr)
            {
                auto clip = clipToXml (*t.slots[s]);
                clip->setAttribute ("slot", (int) s);
                e->addChildElement (clip.release());
            }
    }
    return xml;
}

inline bool songFromXml (const juce::XmlElement& xml, Song& song)
{
    if (! xml.hasTagName ("GridPlay"))
        return false;

    song = {};
    song.tempo = juce::jlimit (20.0, 999.0, xml.getDoubleAttribute ("tempo", 120.0));
    song.quantize = xml.getDoubleAttribute ("quantize", beatsPerBar);
    song.recordBars = xml.getIntAttribute ("recordBars", 0);
    song.masterVolume = (float) xml.getDoubleAttribute ("master", 0.85);

    if (auto* scenes = xml.getChildByName ("Scenes"))
        for (auto* s : scenes->getChildWithTagNameIterator ("Scene"))
            song.scenes.push_back ({ s->getStringAttribute ("name") });

    if (auto* tracks = xml.getChildByName ("Tracks"))
        for (auto* e : tracks->getChildWithTagNameIterator ("Track"))
        {
            Track t;
            t.name = e->getStringAttribute ("name");
            t.colour = juce::Colour::fromString (e->getStringAttribute ("colour", palette()[0].toString()));
            t.inputDevice = e->getStringAttribute ("input");
            t.inputEnabled = e->getBoolAttribute ("inputEnabled", true);
            t.inputChannel = e->getIntAttribute ("inputChannel", 0);
            const auto output = e->getStringAttribute ("output");
            t.outputType = output == "plugin" ? OutputType::plugin : output == "midi" ? OutputType::midiPort : OutputType::none;
            t.outputPort = e->getStringAttribute ("port");
            t.outputChannel = juce::jlimit (1, 16, e->getIntAttribute ("outputChannel", 1));
            t.volume = (float) e->getDoubleAttribute ("volume", 0.85);
            t.mute = e->getBoolAttribute ("mute");
            t.solo = e->getBoolAttribute ("solo");
            t.arm = e->getBoolAttribute ("arm");
            t.drumMap = e->getBoolAttribute ("drumMap");
            if (auto* p = e->getChildByName ("Plugin"))
            {
                t.pluginName = p->getStringAttribute ("name");
                t.pluginState.fromBase64Encoding (p->getStringAttribute ("state"));
                if (auto* desc = p->getChildByName ("PLUGIN"))
                    t.pluginDescription = desc->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader());
            }
            t.slots.resize (song.scenes.size());
            for (auto* c : e->getChildWithTagNameIterator ("Clip"))
            {
                const int slot = c->getIntAttribute ("slot", -1);
                if (juce::isPositiveAndBelow (slot, (int) t.slots.size()))
                    t.slots[(size_t) slot] = clipFromXml (*c);
            }
            song.tracks.push_back (std::move (t));
        }

    return true;
}

//==============================================================================
// Fichiers MIDI standard (.mid) : toutes les notes de toutes les pistes du fichier.
inline ClipPtr clipFromMidiFile (const juce::File& file)
{
    juce::FileInputStream stream (file);
    juce::MidiFile midi;
    if (! stream.openedOk() || ! midi.readFrom (stream))
        return {};

    const int ticks = midi.getTimeFormat();
    if (ticks <= 0)
        return {};   // format SMPTE, rare : non géré

    auto clip = std::make_shared<Clip>();
    clip->name = file.getFileNameWithoutExtension();
    double end = 0.0;
    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence seq (*midi.getTrack (t));
        seq.updateMatchedPairs();
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            const auto* ev = seq.getEventPointer (i);
            if (! ev->message.isNoteOn())
                continue;
            const double start = ev->message.getTimeStamp() / ticks;
            const double stop = ev->noteOffObject != nullptr ? ev->noteOffObject->message.getTimeStamp() / ticks : start + 0.25;
            clip->notes.push_back ({ start, juce::jmax (1.0 / 64.0, stop - start), ev->message.getNoteNumber(), (int) ev->message.getVelocity() });
            end = juce::jmax (end, stop);
        }
    }
    std::sort (clip->notes.begin(), clip->notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    clip->length = juce::jmax ((double) beatsPerBar, std::ceil (end / beatsPerBar - 1.0e-6) * beatsPerBar);
    return clip;
}

inline bool clipToMidiFile (const Clip& clip, double tempo, const juce::File& file)
{
    constexpr int ticks = 960;
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / tempo)), 0.0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (beatsPerBar, 4), 0.0);
    for (const auto& n : clip.notes)
    {
        if (n.muted)
            continue;
        seq.addEvent (juce::MidiMessage::noteOn (1, n.pitch, (juce::uint8) n.velocity), std::round (n.start * ticks));
        seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch), std::round ((n.start + n.length) * ticks));
    }
    seq.addEvent (juce::MidiMessage::endOfTrack(), std::round (clip.length * ticks));
    seq.sort();
    seq.updateMatchedPairs();

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (ticks);
    midi.addTrack (seq);
    file.deleteFile();
    juce::FileOutputStream out (file);
    return out.openedOk() && midi.writeTo (out);
}
} // namespace gridplay
