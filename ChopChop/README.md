# Chop Chop

**Repeater de voix distordu** : hache une voix au tempo, la fait bégayer, monter d'une
octave, repartir à l'envers, la salit et la fait pomper avec les drums. Pour ce son de
voix découpées, sales et très dynamiques qu'on entend dans la house et l'UK garage
d'aujourd'hui.

Formats : VST3 (Cubase, Nuendo, Ableton Live, Studio One, FL Studio, Reaper…),
AU (Logic, GarageBand), CLAP (Bitwig, Reaper) et LV2 (Ardour, Carla), pour macOS
(Apple Silicon et Intel), Windows et Linux. Sans latence.

## Comment ça marche

Chop Chop écoute la voix en continu. Quand un pas le demande, il rejoue le dernier bout
de voix (de 1/4 à 1/64 de temps), calé sur le début d'une syllabe (SNAP) pour ne pas
couper un mot en plein milieu.

**Les pas** (cliquer pour changer, glisser pour peindre, clic droit : menu, double-clic : vide)

| Pas | Effet |
|---|---|
| THRU | la voix passe telle quelle |
| REPEAT | le bout est répété ; plusieurs REPEAT de suite gardent le même bout |
| ROLL | répété en boucles deux fois plus courtes à chaque tour (le « roulement ») |
| REVERSE | le bout à l'envers |
| +12 / -12 | le bout une octave au-dessus ou en dessous, par la vitesse (comme un sampler) |
| MUTE | silence (effet gate) |

**Déclenchement** (TRIGGER)
- **PATTERN** : les 16 pas défilent au tempo du projet (en 1/8, 1/16 ou 1/32).
- **DRUMS** : la piste de drums, envoyée sur l'entrée sidechain, déclenche les bouts :
  chaque coup (kick, caisse claire ou tout) joue le pas actif suivant, pendant LENGTH.
  Les bouts de voix tombent pile avec la batterie.
- **MIDI** : chaque note rejoue le dernier bout à sa hauteur (do 3 = hauteur d'origine) :
  on joue la voix au clavier.

**Réglages**
- REPEAT : SIZE (longueur du bout), RISE (la hauteur monte ou descend à chaque répétition),
  DECAY (le volume baisse à chaque répétition), CHANCE (probabilité qu'un pas se déclenche),
  PITCH (hauteur de tous les bouts), SNAP (caler sur les syllabes).
- DIRT : WARM (saturation), FUZZ (écrêtage asymétrique) ou CRUSH (bits et échantillonnage
  réduits) ; DRIVE et DRUM DRIVE (saleté en plus sur chaque coup de drums : la distorsion respire).
- FILTER : passe-bas, passe-haut ou passe-bande (effet téléphone) ; DRUM MOD : les drums
  ouvrent ou ferment le filtre.
- PUMP : la voix baisse sur chaque kick des drums (RELEASE : vitesse de remontée).
- SPACE : delay au tempo (aller-retour gauche/droite, échos assombris) et reverb.
- OUTPUT : MIX (voix d'origine / traitée), OUTPUT.

Les drums servent aussi en mode PATTERN et MIDI : pour DRUM DRIVE, DRUM MOD et PUMP.

Presets : bégaiement de fin de mesure, voix hachée sur les drums, le kick découpe,
chipmunk rave, téléphone sale, rembobinage, pluie de répétitions, gate hachée,
distorsion qui respire, voix au clavier (MIDI).

## Dans Cubase

1. Insérer **Chop Chop** sur la piste de voix.
2. Dans la fenêtre du plugin, activer le **Side-Chain** (bouton en haut de la fenêtre).
3. Sur la piste (ou le groupe) de drums, ajouter un **envoi (Send)** vers « Chop Chop – Side-Chain ».
4. Choisir un preset, lancer la lecture : le séquenceur suit le tempo du projet.

Déclenchement MIDI : créer une piste MIDI et choisir **Chop Chop** comme sortie.

Astuce : automatiser MIX ou le pas de séquenceur dans Cubase pour ne hacher que
certains mots, comme dans les morceaux du genre.

## Compiler et tester

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/RenderTest_artefacts/Release/RenderTest      # tests du moteur
```

Options de développement : `-DCHOPCHOP_SNAPSHOT=ON` (capture PNG de l'interface,
`Snapshot image.png [preset]`) et `-DCHOPCHOP_HOSTTEST=ON` (charge le VST3 et l'AU
compilés comme un DAW : `HostTest "Chop Chop.vst3" au`).

Licence GNU AGPL v3.
