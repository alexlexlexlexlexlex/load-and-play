// Tests du séquenceur (lancement calé, boucles, enregistrement, scènes) et du
// protocole de l'APC mini, sans carte son ni appareil MIDI.

#include "../Source/Session.h"
#include "../Source/ApcProtocol.h"

#include <iostream>

using namespace gridplay;

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok    " : "  ÉCHEC ") << what.toRawUTF8() << std::endl;
    if (! ok)
        ++failures;
}

constexpr double rate = 48000.0;
constexpr int block = 480;                    // 10 ms
constexpr double samplesPerBeat = rate / 2.0; // 120 BPM

struct Event { long long sample; bool on; int pitch; int velocity; };

// Fait tourner la session sur n blocs et note tout ce qui sort de la piste demandée.
struct Runner
{
    Session session;
    long long time = 0;
    std::vector<std::vector<Event>> events;
    std::vector<juce::MidiBuffer> outputs;

    Runner() { session.prepare (rate); session.song.tempo = 120.0; }

    void run (int blocks, std::vector<LiveEvent> live = {})
    {
        for (int b = 0; b < blocks; ++b)
        {
            session.process (block, b == 0 ? live : std::vector<LiveEvent> {}, outputs);
            events.resize (outputs.size());
            for (size_t t = 0; t < outputs.size(); ++t)
                for (const auto meta : outputs[t])
                {
                    const auto m = meta.getMessage();
                    if (m.isNoteOnOrOff())
                        events[t].push_back ({ time + meta.samplePosition, m.isNoteOn(), m.getNoteNumber(), m.getVelocity() });
                }
            time += block;
        }
    }

    // Nombre de blocs pour atteindre un temps donné (en noires).
    int blocksUntil (double beat) const
    {
        return (int) std::ceil ((beat * samplesPerBeat - (double) time) / block);
    }

    std::vector<Event> ons (int track) const
    {
        std::vector<Event> r;
        for (const auto& e : events[(size_t) track])
            if (e.on) r.push_back (e);
        return r;
    }
};

ClipPtr fourNotes (int firstPitch)
{
    auto clip = std::make_shared<Clip>();
    clip->length = 4.0;
    for (int i = 0; i < 4; ++i)
        clip->notes.push_back ({ (double) i, 0.5, firstPitch + i, 100 });
    return clip;
}

long long at (double beat) { return (long long) std::llround (beat * samplesPerBeat); }
} // namespace

int main()
{
    //--------------------------------------------------------------------------
    std::cout << "Lancement et boucle" << std::endl;
    {
        Runner r;
        r.session.setClip (0, 0, fourNotes (60));
        r.session.launchSlot (0, 0);
        check (r.session.isPlaying(), "lancer un clip démarre le transport");
        r.run (r.blocksUntil (8.0));
        const auto ons = r.ons (0);
        check (ons.size() == 8, "8 notes en deux tours de boucle (" + juce::String ((int) ons.size()) + ")");
        bool timing = ons.size() == 8;
        for (size_t i = 0; i < ons.size() && timing; ++i)
            timing = ons[i].sample == at ((double) i) && ons[i].pitch == 60 + (int) (i % 4);
        check (timing, "chaque note tombe sur son temps, au sample près");
        int offs = 0;
        for (const auto& e : r.events[0]) if (! e.on) ++offs;
        check (offs == 8, "chaque note est relâchée");
        check (r.session.getSlotState (0, 0) == SlotState::playing, "la case est « en lecture »");
    }

    //--------------------------------------------------------------------------
    std::cout << "Quantification : changement de clip à la mesure suivante" << std::endl;
    {
        Runner r;
        r.session.setClip (0, 0, fourNotes (60));
        r.session.setClip (0, 1, fourNotes (72));
        r.session.launchSlot (0, 0);
        r.run (r.blocksUntil (1.25));
        r.session.launchSlot (0, 1);
        check (r.session.getSlotState (0, 1) == SlotState::playQueued, "le second clip attend la mesure");
        r.run (r.blocksUntil (6.0));
        bool switched = false, early = false;
        for (const auto& e : r.ons (0))
        {
            if (e.pitch >= 72 && e.sample == at (4.0)) switched = true;
            if (e.pitch >= 72 && e.sample < at (4.0)) early = true;
            if (e.pitch < 72 && e.sample >= at (4.0)) early = true;
        }
        check (switched && ! early, "le second clip commence pile au temps 4, le premier s'arrête");
    }

    std::cout << "Quantification à la noire, et sans quantification" << std::endl;
    {
        Runner r;
        r.session.song.quantize = 1.0;
        r.session.setClip (0, 0, fourNotes (60));
        r.session.setClip (1, 0, fourNotes (72));
        r.session.launchSlot (0, 0);
        r.run (r.blocksUntil (1.5));
        r.session.launchSlot (1, 0);
        r.run (r.blocksUntil (3.0));
        const auto ons = r.ons (1);
        check (! ons.empty() && ons[0].sample == at (2.0), "à la noire : départ au temps 2");

        Runner f;
        f.session.song.quantize = 0.0;
        f.session.setClip (0, 0, fourNotes (60));
        f.session.setClip (1, 0, fourNotes (72));
        f.session.launchSlot (0, 0);
        f.run (f.blocksUntil (1.5));
        const long long launched = f.time;
        f.session.launchSlot (1, 0);
        f.run (5);
        const auto ons2 = f.ons (1);
        check (! ons2.empty() && ons2[0].sample == launched, "sans quantification : départ immédiat");
    }

    //--------------------------------------------------------------------------
    std::cout << "Arrêt d'une piste et scènes" << std::endl;
    {
        Runner r;
        for (int t = 0; t < 3; ++t)
        {
            r.session.setClip (t, 0, fourNotes (40 + 12 * t));
            r.session.setClip (t, 1, fourNotes (41 + 12 * t));
        }
        r.session.launchScene (0);
        r.run (r.blocksUntil (2.0));
        check (r.session.isSceneActive (0), "la scène 1 joue sur les trois pistes");
        r.session.stopTrack (1);
        r.run (r.blocksUntil (5.0));
        bool stopped = true;
        for (const auto& e : r.ons (1)) if (e.sample >= at (4.0)) stopped = false;
        check (stopped && r.session.getPlayingSlot (1) == -1, "la piste 2 s'arrête à la mesure");

        r.session.launchScene (1);
        r.run (r.blocksUntil (9.0));
        check (r.session.getPlayingSlot (0) == 1 && r.session.getPlayingSlot (1) == 1 && r.session.getPlayingSlot (2) == 1,
               "la scène 2 relance toutes les pistes, même celle qui était arrêtée");

        r.session.stopAllClips();
        r.run (r.blocksUntil (13.0));
        check (! r.session.isTrackActive (0) && ! r.session.isTrackActive (2), "tout s'arrête");
    }

    //--------------------------------------------------------------------------
    std::cout << "Enregistrement" << std::endl;
    {
        Runner r;
        r.session.song.tracks[0].arm = true;
        r.session.setClip (1, 0, fourNotes (30));      // une autre piste fait tourner le transport
        r.session.launchSlot (1, 0);
        r.run (r.blocksUntil (1.0));
        r.session.launchSlot (0, 2);                     // case vide d'une piste armée
        check (r.session.getSlotState (0, 2) == SlotState::recordQueued, "case vide d'une piste armée : enregistrement en attente");
        r.run (r.blocksUntil (4.0) + 1);
        check (r.session.getSlotState (0, 2) == SlotState::recording, "l'enregistrement démarre à la mesure");

        // Do au temps 5 (relâché au temps 6), mi au temps 7.5 (relâché à 8).
        r.run (r.blocksUntil (5.0));
        r.run (1, { { 0, juce::MidiMessage::noteOn (3, 60, (juce::uint8) 90) } });
        r.run (r.blocksUntil (6.0));
        r.run (1, { { 0, juce::MidiMessage::noteOff (3, 60) } });
        r.run (r.blocksUntil (7.5));
        r.run (1, { { 0, juce::MidiMessage::noteOn (3, 64, (juce::uint8) 70) } });
        r.run (r.blocksUntil (8.0) - 1);
        r.session.launchSlot (0, 2);                     // re-cliquer : fin de l'enregistrement à la mesure
        r.run (1);
        r.run (2, { { 0, juce::MidiMessage::noteOff (3, 64) } });

        auto clip = r.session.song.clipAt (0, 2);
        check (clip != nullptr && std::abs (clip->length - 4.0) < 1e-9, "clip d'une mesure");
        bool notesOk = clip != nullptr && clip->notes.size() == 2;
        if (notesOk)
        {
            const auto& a = clip->notes[0];
            const auto& b = clip->notes[1];
            const double tolerance = block / samplesPerBeat;
            notesOk = a.pitch == 60 && std::abs (a.start - 1.0) <= tolerance && std::abs (a.length - 1.0) <= tolerance && a.velocity == 90
                   && b.pitch == 64 && std::abs (b.start - 3.5) <= tolerance && b.velocity == 70;
        }
        check (notesOk, "les deux notes sont enregistrées à leur place, avec leur vélocité");
        check (r.session.getSlotState (0, 2) == SlotState::playing, "la boucle enregistrée joue aussitôt");

        // Le jeu au clavier passe aussi vers l'instrument, sur le canal 1.
        bool monitored = false;
        for (const auto& e : r.ons (0)) if (e.pitch == 60 && e.velocity == 90) monitored = true;
        check (monitored, "le clavier s'entend pendant l'enregistrement");

        r.run (r.blocksUntil (13.5));
        int replay = 0;
        for (const auto& e : r.ons (0)) if (e.pitch == 60 && e.sample == at (9.0)) ++replay;
        check (replay == 1, "la note enregistrée rejoue au tour suivant (temps 9)");
    }

    std::cout << "Enregistrement à longueur fixe" << std::endl;
    {
        Runner r;
        r.session.song.recordBars = 2;
        r.session.song.tracks[0].arm = true;
        r.session.launchSlot (0, 0);
        r.run (r.blocksUntil (1.0));
        r.run (1, { { 0, juce::MidiMessage::noteOn (1, 50, (juce::uint8) 100) } });
        r.run (r.blocksUntil (2.0));
        r.run (1, { { 0, juce::MidiMessage::noteOff (1, 50) } });
        r.run (r.blocksUntil (8.5));
        auto clip = r.session.song.clipAt (0, 0);
        check (clip != nullptr && std::abs (clip->length - 8.0) < 1e-9 && clip->notes.size() == 1, "deux mesures, puis la boucle joue seule");
        check (r.session.getSlotState (0, 0) == SlotState::playing, "lecture après la longueur fixe");
    }

    //--------------------------------------------------------------------------
    std::cout << "Mute, solo, arrêt du transport" << std::endl;
    {
        Runner r;
        r.session.setClip (0, 0, fourNotes (60));
        r.session.setClip (1, 0, fourNotes (72));
        r.session.launchScene (0);
        r.run (r.blocksUntil (0.25));
        r.session.song.tracks[1].solo = true;
        r.run (r.blocksUntil (4.0));
        int track0 = 0;
        for (const auto& e : r.ons (0)) if (e.sample > 0) ++track0;
        check (track0 == 0, "solo sur la piste 2 : la piste 1 se tait");
        bool releasedOnSolo = false;
        for (const auto& e : r.events[0]) if (! e.on && e.pitch == 60 && e.sample <= at (0.25) + block) releasedOnSolo = true;
        check (releasedOnSolo, "sa note en cours est relâchée tout de suite");

        r.session.stop();
        r.run (1);
        check (! r.session.isPlaying() && ! r.session.isTrackActive (1), "stop : transport et clips arrêtés");
        int pending = 0;
        for (const auto& e : r.events[1]) pending += e.on ? 1 : -1;
        check (pending == 0, "aucune note ne reste coincée");
    }

    //--------------------------------------------------------------------------
    std::cout << "Structure : insérer une scène pendant la lecture" << std::endl;
    {
        Runner r;
        r.session.setClip (0, 2, fourNotes (60));
        r.session.launchSlot (0, 2);
        r.run (5);
        r.session.insertScene (0, { "Nouvelle" });
        check (r.session.getPlayingSlot (0) == 3 && r.session.getSlotState (0, 3) == SlotState::playing, "le clip qui joue suit sa case");
        r.session.removeScene (3);
        r.run (1);
        check (r.session.getPlayingSlot (0) == -1, "supprimer sa scène l'arrête");
    }

    std::cout << "Sauvegarde et fichiers MIDI" << std::endl;
    {
        Song song = makeDefaultSong();
        song.tempo = 128.0;
        song.tracks[2].name = "Basse";
        song.tracks[2].outputType = OutputType::midiPort;
        song.tracks[2].outputPort = "IAC";
        song.tracks[2].outputChannel = 5;
        song.tracks[2].slots[4] = fourNotes (36);
        song.tracks[2].slots[4]->name = "Riff";
        auto xml = songToXml (song);
        Song back;
        const bool ok = songFromXml (*xml, back);
        check (ok && back.tempo == 128.0 && back.tracks[2].name == "Basse" && back.tracks[2].outputChannel == 5
                  && back.tracks[2].outputType == OutputType::midiPort && back.clipAt (2, 4) != nullptr
                  && back.clipAt (2, 4)->notes.size() == 4 && back.clipAt (2, 4)->name == "Riff",
               "le morceau se relit à l'identique");

        auto file = juce::File::createTempFile (".mid");
        auto clip = fourNotes (48);
        clip->length = 8.0;
        clip->notes.push_back ({ 6.5, 1.0, 55, 64 });
        const bool written = clipToMidiFile (*clip, 120.0, file);
        auto read = clipFromMidiFile (file);
        check (written && read != nullptr && read->notes.size() == 5 && std::abs (read->notes[4].start - 6.5) < 1e-6
                  && read->notes[4].velocity == 64 && std::abs (read->length - 8.0) < 1e-9,
               "export puis import d'un fichier .mid");
        file.deleteFile();
    }

    //--------------------------------------------------------------------------
    std::cout << "APC mini" << std::endl;
    {
        using namespace apc;
        check (modelFromName ("APC MINI") == Model::mk1 && modelFromName ("APC mini mk2 Control") == Model::mk2
                  && modelFromName ("Arturia KeyStep") == Model::none, "reconnaît mk1, mk2 et le reste");
        check (portScore ("APC mini mk2 Control") > portScore ("APC mini mk2 Notes")
                  && portScore ("APC mini mk2") > portScore ("MIDIIN2 (APC mini mk2)"), "choisit le port de contrôle du mk2");

        auto pad = parse (Model::mk2, juce::MidiMessage::noteOn (1, 56, (juce::uint8) 127));
        check (pad.kind == Input::Kind::pad && pad.row == 0 && pad.column == 0 && pad.pressed, "note 56 = pad en haut à gauche");
        auto corner = parse (Model::mk1, juce::MidiMessage::noteOn (1, 7, (juce::uint8) 127));
        check (corner.kind == Input::Kind::pad && corner.row == 7 && corner.column == 7, "note 7 = pad en bas à droite");
        check (parse (Model::mk1, juce::MidiMessage::noteOn (1, 84, (juce::uint8) 127)).kind == Input::Kind::sceneButton
                  && parse (Model::mk1, juce::MidiMessage::noteOn (1, 84, (juce::uint8) 127)).index == 2, "mk1 : note 84 = scène 3");
        check (parse (Model::mk2, juce::MidiMessage::noteOn (1, 103, (juce::uint8) 127)).kind == Input::Kind::trackButton
                  && parse (Model::mk2, juce::MidiMessage::noteOn (1, 103, (juce::uint8) 127)).index == 3, "mk2 : note 103 = bouton de piste 4");
        check (parse (Model::mk2, juce::MidiMessage::noteOn (1, 122, (juce::uint8) 127)).kind == Input::Kind::shift, "mk2 : SHIFT");
        auto fader = parse (Model::mk1, juce::MidiMessage::controllerEvent (1, 56, 127));
        check (fader.kind == Input::Kind::fader && fader.index == 8 && fader.value == 1.0f, "fader master au maximum");
        check (parse (Model::mk2, juce::MidiMessage::noteOff (1, 10)).pressed == false, "relâchement d'un pad");

        auto mk1 = padMessage (Model::mk1, gridNote (0, 0), Pad::playing, juce::Colours::red, true);
        check (mk1.getNoteNumber() == 56 && mk1.getVelocity() == 1 && mk1.getChannel() == 1, "mk1 : clip qui joue = vert");
        check (padMessage (Model::mk1, 0, Pad::playQueued, {}, false).getVelocity() == 2, "mk1 : en attente = vert clignotant");
        auto mk2 = padMessage (Model::mk2, 0, Pad::recording, {}, true);
        check (mk2.getVelocity() == 5 && mk2.getChannel() == 7, "mk2 : enregistrement = rouge à 100 %");
        check (padMessage (Model::mk2, 0, Pad::clip, juce::Colour (0xff1aff2f), true).getVelocity() == 21, "mk2 : clip vert = vert dans la palette");
        check (padMessage (Model::mk2, 0, Pad::off, {}, true).getVelocity() == 0, "mk2 : case vide éteinte");
    }

    std::cout << (failures == 0 ? juce::String ("Tout est bon.") : juce::String (failures) + " échec(s).").toRawUTF8() << std::endl;
    return failures == 0 ? 0 : 1;
}
