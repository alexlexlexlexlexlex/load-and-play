# Grid Play

Lanceur de boucles **MIDI** façon vue session d'Ableton Live, pour jouer ses instruments :
une grille de clips (pistes en colonnes, scènes en lignes), lancés en rythme, en boucle.
Pas d'audio enregistré ni bouclé : uniquement du MIDI, vers des instruments virtuels
chargés dans Grid Play ou vers des ports MIDI.

macOS (Apple Silicon et Intel), Windows et Linux (x64 et ARM64), comme les instruments.

*Nom provisoire. Étape 1 : la matrice de lancement. Étape suivante : l'éditeur de notes.*

## La grille

- **Clic sur le triangle d'un clip** : il démarre au prochain temps fort (réglage
  « Lancement » : aucune, 1/16 … 4 mesures ; 1 mesure par défaut) et tourne en boucle.
  Le clip qui jouait sur la même piste s'arrête au même moment.
- **Case vide** : arrête la piste ; si la piste est **armée** (●), enregistre le clavier.
  Re-cliquer finit l'enregistrement au temps fort suivant et la boucle joue aussitôt.
  « Enregistrement » : libre, ou longueur fixe (1, 2, 4, 8 mesures).
- **Scènes** (colonne de droite) : lancent toute la ligne. « Tout arrêter » en bas.
- **Double-clic sur une case vide** : nouveau clip vide. **Glisser un .mid** sur une case :
  l'importe (plusieurs fichiers : un par case). Clic droit : renommer, couleur, dupliquer,
  copier, coller, supprimer, exporter en .mid, nouveau clip de 1 à 8 mesures.
- **Glisser un clip** : le déplace (Alt ou Cmd : le copie).
- Clic droit sur une tête de piste ou une scène : renommer, couleur, insérer, supprimer.
  Clic sur le « + » : nouvelle piste ou nouvelle scène.

## Les pistes (console du bas)

- **Entrée** : tous les claviers MIDI, un seul, ou aucun ; un canal ou tous. Le clavier joue
  la piste quand elle est armée (armer une piste désarme les autres, Cmd pour en armer plusieurs).
- **Sortie** :
  - un **instrument virtuel** (VST3, AU sur Mac, LV2 sur Linux : LP-60, Model Play…),
    bouton « Ouvrir » pour sa fenêtre ;
  - un **port MIDI** et un canal : synthé matériel, ou un autre logiciel. Sur Mac et Linux,
    Grid Play crée un port virtuel « Grid Play » que Cubase, Ableton… voient comme une entrée
    (sous Windows, passer par loopMIDI).
- **M** (mute), **S** (solo, Cmd pour plusieurs), volume (gain de l'instrument, ou CC 7 sur un port MIDI).

Les instruments installés se trouvent avec **Instruments** (puis « Options… » →
« Scan for new or updated VST3 plug-ins », et AudioUnit sur Mac). La carte son se choisit
dans **Audio / MIDI**. Les claviers MIDI branchés sont pris automatiquement.

## Akai APC mini (mk1 et mk2)

Reconnu dès qu'il est branché (état en haut à droite).

| APC mini | Grid Play |
|---|---|
| Pads 8 x 8 | lancent les clips des 8 premières pistes x 8 premières scènes |
| Boutons du bas | arrêtent la piste · avec SHIFT : armer / désarmer |
| Boutons de droite | lancent la scène · avec SHIFT : tout arrêter |
| Faders 1 à 8 | volume des pistes · fader 9 : master |

LED : couleur du clip (jaune sur le mk1), vert = en lecture, vert clignotant = en attente,
rouge = enregistrement, rouge clignotant = enregistrement en attente.

## Raccourcis (proches d'Ableton Live)

| Touche | Action |
|---|---|
| Espace | lecture / stop |
| Flèches, Entrée | se déplacer dans la grille, lancer la case |
| Suppr | supprimer le clip |
| Cmd D / C / V / R | dupliquer, copier, coller, renommer le clip |
| Cmd Maj T, Cmd I | insérer une piste MIDI, une scène |
| Cmd N / O / S / Maj S | nouveau, ouvrir, enregistrer, enregistrer sous |
| Cmd , | réglages audio / MIDI |

(Ctrl à la place de Cmd sous Windows et Linux.) Les morceaux s'enregistrent en `.gridplay`
(XML lisible), avec l'état des instruments.

## Compiler et tester

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/SessionTest_artefacts/Release/SessionTest     # séquenceur et protocole de l'APC mini
```

Outil de développement : `"Grid Play" --snapshot image.png morceau.gridplay` lance la
première scène et enregistre une image de la fenêtre ; avec la variable
`GRIDPLAY_TEST_PLUGIN=<chemin d'un .vst3>`, charge cet instrument sur la piste 1 et
affiche la crête de sortie.

## Organisation

| Fichier | Rôle |
|---|---|
| `Source/Model.h` | Morceau, pistes, scènes, clips ; sauvegarde XML ; fichiers MIDI |
| `Source/Session.*` | Séquenceur : transport, lancement calé, boucles, enregistrement (au sample près) |
| `Source/AudioEngine.*` | Carte son, claviers, instruments virtuels, ports MIDI |
| `Source/ApcProtocol.h`, `ApcMini.*` | Protocole et pilotage de l'APC mini mk1 / mk2 |
| `Source/SessionView.*` | La grille et la console |
| `Source/MainComponent.*`, `Main.cpp` | Fenêtre, transport, menus, raccourcis, fichiers |

Licence GNU AGPL v3.
