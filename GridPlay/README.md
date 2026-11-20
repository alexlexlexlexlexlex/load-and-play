# Grid Play

Lanceur de boucles **MIDI** façon vue session d'Ableton Live, pour jouer ses instruments :
une grille de clips (pistes en colonnes, scènes en lignes), lancés en rythme, en boucle.
Pas d'audio enregistré ni bouclé : uniquement du MIDI, vers des instruments virtuels
chargés dans Grid Play ou vers des ports MIDI.

macOS (Apple Silicon et Intel), Windows et Linux (x64 et ARM64), comme les instruments.

*Nom provisoire. Étapes 1 et 2 : la matrice de lancement et l'éditeur de notes.*

## La grille

- **Clic sur le triangle d'un clip** : il démarre au prochain temps fort (réglage
  « Lancement » : aucune, 1/16 … 4 mesures ; 1 mesure par défaut) et tourne en boucle.
  Le clip qui jouait sur la même piste s'arrête au même moment.
- **Case vide** : arrête la piste ; si la piste est **armée** (●), enregistre le clavier.
  Re-cliquer finit l'enregistrement au temps fort suivant et la boucle joue aussitôt.
  « Enregistrement » : libre, ou longueur fixe (1, 2, 4, 8 mesures).
- **Scènes** (colonne de droite) : lancent toute la ligne. « Tout arrêter » en bas.
- **Double-clic sur un clip** (ou Cmd E) : ouvre l'éditeur de notes.
- **Double-clic sur une case vide** : nouveau clip vide. **Glisser un .mid** sur une case :
  l'importe (plusieurs fichiers : un par case). Clic droit : renommer, couleur, dupliquer,
  copier, coller, supprimer, exporter en .mid, nouveau clip de 1 à 8 mesures.
- **Glisser un clip** : le déplace (Alt ou Cmd : le copie).
- **Double-clic sur le nom d'une piste ou d'une scène** : on le renomme sur place (Entrée pour
  valider, Échap pour annuler). Clic droit : renommer, couleur, insérer, supprimer.
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

**Tempo** : cliquer sur la valeur et glisser vers le haut ou le bas (Maj : au centième), molette,
ou boutons - / + d'un BPM (maintenus : ça défile ; Maj : 0,1). Double-clic pour taper la valeur.
Un second clic sur **Instruments** ou **Audio / MIDI** referme la fenêtre.

Les instruments installés se trouvent avec **Instruments** (puis « Options… » →
« Scan for new or updated VST3 plug-ins », et AudioUnit sur Mac). La carte son se choisit
dans **Audio / MIDI**. Les claviers MIDI branchés sont pris automatiquement.

## L'éditeur de notes

Sur le modèle du Key Editor de Cubase : barre d'outils, ligne d'infos, clavier à gauche,
règle, grille des notes et piste des vélocités. Les notes sont colorées selon leur vélocité
(bleu doux, vert, rouge fort) ; ce qui dépasse la fin de la boucle est assombri.

| Outil (touche) | Effet |
|---|---|
| Sélection (1) | clic : sélectionner ; glisser : déplacer (Alt : copier, Cmd : un seul sens) ; bords : changer le début ou la fin ; cadre sur le vide : sélection multiple ; Maj / Cmd : ajouter ; Cmd Maj glisser : vélocité ; double-clic : toutes les notes de cette hauteur |
| Ciseaux (3) | couper la note (et les notes sélectionnées) à cet endroit |
| Colle (4) | joindre la note à la suivante de même hauteur |
| Gomme (5) | effacer (cliquer ou glisser) |
| Zoom (6) | clic : agrandir ; Alt-clic : réduire |
| Sourdine (7) | la note reste affichée mais ne joue plus |
| Crayon (8) | clic : nouvelle note (longueur choisie, vélocité choisie), glisser pour l'allonger ; clic sur une note : l'effacer |

- **Clic droit** : la boîte à outils de Cubase. On peut garder le bouton appuyé, glisser sur un
  outil ou une commande (quantifier, legato, sourdine, dupliquer…) et relâcher ; ou cliquer, puis choisir.
  Chaque bouton de la barre d'outils explique ce qu'il fait quand on le survole.
- **Lecture comme dans Cubase** : ▶ (ou Espace) joue le clip en boucle **depuis le curseur**,
  la ligne qui avance, toujours calé sur la même grille que le reste : si d'autres clips tournent,
  il part au prochain moment où la mesure arrive à la position du curseur (▶ clignote en attendant) ;
  si rien ne joue, le transport démarre à cette position. Cliquer ou glisser dans la règle déplace
  le curseur, même pendant la lecture : toute la lecture saute avec lui, les clips restent calés.
  ■ arrête et laisse le curseur où il est ; un second ■ (ou Début) le ramène au début.
  La vue suit le curseur pendant la lecture.
- **Zoom dans la règle**, comme Cubase : cliquer dans la règle et garder appuyé, glisser vers le
  bas pour zoomer, vers le haut pour dézoomer (la mesure cliquée reste sous la souris).
- **Quantification** (comme Cubase, elle sert aussi au calage) : valeur de 1/1 à 1/64 et triolets,
  **Q** pour quantifier, et **…** pour le panneau : swing, force (rapprocher sans coller à la
  grille), fins de notes.
- **Batterie** (drum map) : les noms des sons General MIDI (grosse caisse, caisse claire,
  charleston…) remplacent le clavier, les notes deviennent des losanges ; les flèches passent
  d'un son à l'autre. Se règle par piste et se garde dans le morceau.
- **Ligne d'infos** : début, fin, longueur (format mesure.temps.double-croche.tick de Cubase),
  hauteur (do 3 = 60) et vélocité de la première note sélectionnée. Cliquer pour taper, molette
  pour changer ; les changements s'appliquent à toutes les notes sélectionnées.
- **Calage** (J pour l'activer / le couper) sur la valeur de quantification.
- **Boucle** : sa longueur dans la barre d'outils, ou le triangle orange à faire glisser dans la règle.
- **Vélocités** : dessiner dans la piste du bas (seulement les notes sélectionnées s'il y en a).
- **Écoute** : on entend les notes en les posant ou en les déplaçant ; clic sur le clavier pour écouter.
- Tout se modifie en direct pendant que le clip tourne.

| Raccourci | Action |
|---|---|
| Haut / Bas (Maj : octave) | transposer la sélection (en batterie : son suivant / précédent) |
| Gauche / Droite (Maj : étendre) | note précédente / suivante |
| Cmd Gauche / Droite | décaler d'un pas de grille |
| Alt Gauche / Droite | raccourcir / allonger d'un pas de grille |
| Cmd A, C, X, V, D | tout sélectionner, copier, couper, coller (au curseur, posé d'un clic dans la règle ou sur le vide), dupliquer |
| Suppr | supprimer |
| Cmd Z, Cmd Maj Z | annuler, rétablir |
| G / H (Maj : vertical) | zoom arrière / avant ; aussi Cmd + molette, Alt + molette |
| Espace | lecture depuis le curseur / stop |
| Entrée | lancer ce clip dans la grille (calé sur la mesure) |
| Début | curseur au début |
| Échap, Cmd W | désélectionner, fermer |

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
| Cmd E | éditer les notes du clip |
| Cmd N / O / S / Maj S | nouveau, ouvrir, enregistrer, enregistrer sous |
| Cmd , | réglages audio / MIDI |

(Ctrl à la place de Cmd sous Windows et Linux.) Les morceaux s'enregistrent en `.gridplay`
(XML lisible), avec l'état des instruments.

## Feuille de route

1. **Matrice de lancement** : faite.
2. **Éditeur de notes** façon éditeur MIDI de Cubase (interface et raccourcis) : fait.
3. **Consolidation** (notes du 20 novembre 2026, à affiner ensemble) :
   - Instruments virtuels en sortie : les jouer et les contrôler comme des instruments, et
     traiter leur son en direct sur la piste.
   - Synthés externes en sortie MIDI : si leur audio revient par la carte son, le retravailler
     sur la même piste que la sortie MIDI, pour piloter et traiter ses synthés en live.
   - **Inserts** d'effets par piste, comme dans Cubase, et sur le **master**.
   - **EQ** sur chaque piste et sur le master, à la façon de Cubase.
   - **Mode DJ / live** : chaque piste envoyable sur un fader de contrôle DJ (EQ Hi / Mid / Lo,
     filtre, volume), un effet loop / roll, et un fader de BPM. À penser pour être simple à
     jouer en live sur un ordinateur.

## Compiler et tester

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/SessionTest_artefacts/Release/SessionTest     # séquenceur et protocole de l'APC mini
```

Outil de développement : `"Grid Play" --snapshot image.png morceau.gridplay` lance la
première scène et enregistre une image de la fenêtre ; avec `GRIDPLAY_TEST_CLIP=piste,scène`,
ouvre aussi l'éditeur sur ce clip (image `-editeur.png`) ; avec la variable
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
| `Source/NoteEditor.*`, `NoteOps.h` | L'éditeur de notes et ses opérations (testées), drum map |
| `Source/Theme.h`, `TransportControls.h` | Couleurs et style (ceux des plugins), transport et tempo |
| `Resources/icon.png`, `Tools/make_icon.m` | L'icône de l'appli et le programme qui la dessine |
| `Source/MainComponent.*`, `Main.cpp` | Fenêtre, transport, menus, raccourcis, fichiers |

Licence GNU AGPL v3.
