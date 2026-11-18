# Model Play — Load & Play

Synthé inspiré du Minimoog Model D, avec des options en plus
(LFO, polyphonie, effets, arpégiateur, MIDI learn). Toutes les options sont
neutres par défaut : le son de base reste celui du Model D émulé, vérifié à
l'échantillon près par les tests.

## Formats et compatibilité

| Format | Logiciels | Installé dans |
|---|---|---|
| AU | Logic Pro, GarageBand, MainStage, Reaper, Ableton Live | `~/Library/Audio/Plug-Ins/Components` |
| VST3 | Ableton Live 10+, Cubase, Studio One, Bitwig, Reaper, FL Studio | `~/Library/Audio/Plug-Ins/VST3` |
| CLAP | Bitwig 4.3+, Reaper 6.71+, FL Studio 2024+ | `~/Library/Audio/Plug-Ins/CLAP` |
| LV2 | Ardour, Carla (et Linux / Raspberry Pi plus tard) | `~/Library/Audio/Plug-Ins/LV2` |
| Appli autonome | sans DAW | `build/ModelD_artefacts/Release/Standalone/Model Play.app` |

Sur Mac, tous les binaires sont universels (Apple Silicon + Intel) et fonctionnent à partir de macOS 10.13.
Windows et Linux (x64 et ARM64) se compilent avec le même code, automatiquement sur GitHub
(`../.github/workflows/plugins.yml`) ou à la main avec CMake.

Formats non fournis :
- VST2 (très vieux logiciels) : Steinberg ne distribue plus le kit de développement depuis 2018.
- AAX (Pro Tools) : demande un compte développeur Avid et une signature payante.
- Logiciels 32 bits : macOS ne les fait plus tourner depuis Catalina (10.15).

## Compiler

Il faut CMake 3.28 ou plus récent et un compilateur C++17 : les Command Line Tools d'Apple sur Mac,
Visual Studio 2022 sous Windows, GCC ou Clang sous Linux (avec les paquets listés dans
`../.github/workflows/plugins.yml`). JUCE et l'extension CLAP sont téléchargés automatiquement
à la première configuration s'ils ne sont pas dans le dossier.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Le plugin AU, VST3 et LV2 est copié automatiquement dans `~/Library/Audio/Plug-Ins`.
Le CLAP se copie à la main depuis `build/ModelD_artefacts/Release/CLAP/`.

Tests :

```bash
./build/RenderTest_artefacts/Release/RenderTest demo.wav Tests/reference.f32   # moteur, options, son de référence
auval -v aumu Mdl1 LdPl                                                         # validation Apple de l'Audio Unit
```

`Tests/reference.f32` est le rendu du son d'origine. Si un changement modifie le
son par défaut, le test échoue.

## Organisation

| Fichier | Rôle |
|---|---|
| `Source/SynthEngine.h` | Le son : oscillateurs, mixer, filtre ladder, enveloppes, glide, LFO, voix. |
| `Source/Effects.h` | Overdrive, chorus, autopan, delay. |
| `Source/Arpeggiator.h` | Arpégiateur et motif de 16 pas. |
| `Source/PluginProcessor.*` | Paramètres, MIDI, MIDI learn, tempo de l'hôte, sauvegarde. |
| `Source/PluginEditor.*` | Interface façon façade Model D, redimensionnable. |
| `Tests/RenderTest.cpp` | Rendu hors DAW + contrôles automatiques. |
| `Tests/Snapshot.cpp` | Capture PNG de l'interface et test du MIDI learn (`-DMODELD_SNAPSHOT=ON`). |

## Le Model D (comme l'original)

- 3 oscillateurs libres, ranges LO / 32' / 16' / 8' / 4' / 2', 6 formes d'onde chacun.
- Osc 3 peut servir de LFO : range LO + interrupteur OSC 3 CTRL coupé.
- Mixer qui sature doucement quand on pousse les niveaux.
- Formes d'onde : la tri-saw des osc 1 et 2 mélange triangle et dent de scie, comme le sélecteur
  du Model D ; ondes pulse larges et étroites à 35 % et 15 % (valeurs couramment citées).
- Filtre 24 dB/oct en échelle, modèle de Huovilainen : saturation tanh à chacun des 4 étages,
  résolu en zero-delay feedback. Emphasis jusqu'à l'auto-oscillation, accordée sur la coupure.
- Comme l'original, monter la résonance creuse les graves (environ -12 dB à Emphasis 7).
- Suivi de clavier 1/3 + 2/3 (KBD 1 / KBD 2).
- Enveloppes attaque / decay / sustain ; l'interrupteur DECAY fait servir le decay de relâchement.
- Priorité à la note la plus basse, déclenchement unique (legato = pas de redéclenchement).
- Molette de modulation (CC1) dosée par MOD MIX entre osc 3 et bruit, envoyée vers OSC MOD et/ou FILTER MOD.

## Les options (neutres par défaut)

| Réglage | Effet | Neutre quand |
|---|---|---|
| RETRIG | relance les enveloppes à chaque note | coupé |
| POLY | 8 voix | coupé |
| MOD AMOUNT | modulation permanente, en plus de la molette | 0 |
| MOD: LFO | la modulation vient du LFO au lieu d'osc 3 / bruit | coupé |
| AFTERTOUCH | la pression ouvre le filtre (jusqu'à 3 octaves) | 0 |
| VELOCITY | la vélocité dose le contour du filtre | 0 |
| FREQUENCY osc 1, FINE TUNE osc 2/3 | accord des oscillateurs | 0 |
| RECTIFY + OFFSET | redresse l'onde autour d'un décalage (son plus riche) | coupé |
| SYNC 2>1 | osc 2 resynchronisé par osc 1 | coupé |
| VCO 3 FILT | osc 3 module le filtre en permanence | 0 |
| ADSR + F./L. RELEASE | relâchement réglable au lieu de l'interrupteur DECAY | coupé |
| LFO | 5 formes, vitesse libre ou calée sur le tempo et la mesure, vers les osc, osc 2, le filtre ou le volume | LEVEL à 0 |
| OVERDRIVE | saturation avec coupe-bas et coupe-haut | coupé |
| CHORUS | chorus façon Juno-60 : MODE I, II ou I+II, LEVEL (10 = mélange égal comme le Juno) | MODE sur Off |
| AUTOPAN (section LFO) | panoramique au rythme du LFO | coupé |
| DELAY | stéréo, libre ou synchro, canal droit 2x plus rapide en option | coupé |
| ARPEGGIATOR | Up, Down, Up/Down, Random, Played ; RANGE de 1 à 4 octaves ; motif de 16 pas avec GATE | coupé |
| TEMPO | tempo utilisé quand aucun hôte n'en donne (appli autonome) | — |
| DRIFT, BASS COMP | instabilité analogique, compensation des graves | réglages d'origine du plugin |

## Le chorus

Valeurs mesurées sur le Juno-60, dont le chorus est très proche de celui du Juno-106
([analyse de pendragon-andyh](https://github.com/pendragon-andyh/Juno60/blob/master/Chorus/README.md)) :
un LFO triangle module deux lignes à retard, celle de droite en opposition de phase.

| Mode | LFO | Retard | Sortie |
|---|---|---|---|
| I | 0,513 Hz | 1,66 à 5,35 ms | stéréo |
| II | 0,863 Hz | 1,66 à 5,35 ms | stéréo |
| I+II | 9,75 Hz | 3,3 à 3,7 ms | mono (presque un vibrato) |

Estimé, faute de mesure : passe-bas 12 dB/oct à 9 kHz avant et après les lignes à
retard, légère saturation du signal retardé. Le souffle des BBD n'est pas imité.

## MIDI learn

1. Cliquer **LEARN** (section MIDI CC).
2. Cliquer un réglage de l'interface : il s'encadre en orange.
3. Bouger un potard ou un fader du contrôleur MIDI : il est associé.
4. Recommencer pour d'autres réglages, puis recliquer **LEARN** pour sortir.

**RESET** efface toutes les associations. Elles sont sauvegardées avec le projet du DAW.

## Licence

Copyright © 2026 Load & Play. Model Play est un logiciel libre sous licence GNU AGPL v3
(voir `LICENSE`) : chacun peut l'utiliser, le modifier et le redistribuer, à condition de
publier ses modifications sous la même licence. Composants tiers : `THIRD-PARTY-NOTICES.md`.
