# Duck Hunt

Effet de **ducking** : une piste baisse de volume quand une autre joue, sans
compresseur. La baisse ne dépend que de la piste de référence (le kick, la voix…),
jamais du niveau de la piste duckée : pas de pompage parasite, pas de couleur de
compresseur.

Formats : VST3 (Cubase, Nuendo, Ableton Live, Studio One, FL Studio, Reaper…),
AU (Logic, GarageBand), CLAP (Bitwig, Reaper) et LV2 (Ardour, Carla), pour macOS
(Apple Silicon et Intel), Windows et Linux.

## Ce qu'il fait

**Déclenchement** (TRIGGER)
- **SIDECHAIN** : la piste de référence, branchée sur l'entrée sidechain, fait baisser la piste.
- **MIDI** : chaque note envoyée au plugin déclenche la courbe ; la vélocité peut doser la baisse.
- **TEMPO** : une baisse à chaque temps (1/1 à 1/16, pointés et triolets), calée sur le DAW.

**Façon de baisser** (MODE)
- **FULL BAND** : tout le signal baisse.
- **3 BANDS** : graves, médiums et aigus baissent chacun selon la même bande de la
  référence, avec un dosage par bande (ex. : le kick ne creuse que les graves de la basse).
  Filtres de Linkwitz-Riley : sans traitement, le son ressort au même niveau, sans latence.
- **SPECTRAL** : la piste ne se creuse qu'aux fréquences où la référence joue, dans
  une zone réglable (FOCUS LOW / HIGH). Idéal pour faire de la place à une voix dans
  des guitares ou un pad. Latence de 1024 échantillons à 44,1/48 kHz, compensée par le DAW.

**Réglages**
- DETECTION : THRESHOLD (niveau de la référence où la baisse commence), RANGE (dB de plus
  pour atteindre la baisse maximale), SC LOW / HIGH CUT (filtrer la référence, ex. ne
  réagir qu'au kick).
- ENVELOPE : ATTACK, HOLD, RELEASE, CURVE (vers + : reste bas puis remonte d'un coup, effet
  « pompe » ; vers − : remonte vite puis traîne).
- DUCK : DEPTH (baisse maximale, jusqu'à 60 dB), LOOKAHEAD (anticipe jusqu'à 20 ms pour
  baisser avant le coup ; ajoute cette latence, compensée par le DAW), INVERT (baisse quand
  la référence se tait : gate).
- OUTPUT : MIX (sec / traité), OUTPUT (gain), LISTEN SC (écouter la référence filtrée).
- Presets : basse sous le kick, graves seulement, place pour la voix, voix radio,
  pompes 1/4 et 1/8, House Rave (le pompage house classique, au tempo), hachures MIDI, gate.

L'écran montre la piste (gris), la référence (orange) et la baisse (blanc, ou une
couleur par bande) ; en mode spectral, les spectres et la baisse par fréquence.

## Dans Cubase

1. Insérer **Duck Hunt** sur la piste à faire baisser.
2. Dans la fenêtre du plugin, activer le **Side-Chain** (bouton en haut de la fenêtre).
3. Sur la piste de référence, ajouter un **envoi (Send)** vers « Duck Hunt – Side-Chain »
   (ou, dans le menu de routage side-chain de la fenêtre du plugin, « Ajouter une source »).
4. Monter THRESHOLD ou baisser le niveau de l'envoi jusqu'à ce que la courbe blanche
   bouge au rythme de la référence.

Déclenchement MIDI : créer une piste MIDI et choisir **Duck Hunt** comme sortie.
Tempo : rien à brancher, le plugin suit le tempo et la position du projet.

## Compiler et tester

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/RenderTest_artefacts/Release/RenderTest      # tests du moteur
```

Options de développement : `-DDUCKHUNT_SNAPSHOT=ON` (capture PNG de l'interface,
`Snapshot image.png [preset]`) et `-DDUCKHUNT_HOSTTEST=ON` (charge le VST3 et l'AU
compilés comme un DAW : `HostTest "Duck Hunt.vst3" au`).

Licence GNU AGPL v3.
