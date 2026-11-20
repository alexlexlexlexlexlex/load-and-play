# Grid Play

Lanceur de boucles **MIDI** façon vue session d'Ableton Live, pour jouer ses instruments :
une grille de clips (pistes en colonnes, scènes en lignes), lancés en rythme, en boucle.
Pas d'audio enregistré ni bouclé : uniquement du MIDI, vers des instruments virtuels
chargés dans Grid Play ou vers des ports MIDI.

macOS (Apple Silicon et Intel), Windows et Linux (x64 et ARM64), comme les instruments.

*Nom provisoire. Étapes 1 et 2 : la matrice de lancement et l'éditeur de notes.*

## Le Hub

Au démarrage, comme le Hub de Cubase : les **projets récents** (avec leur date, et une recherche),
**Créer un projet**, **Sélectionner un fichier…**, et les réglages de départ : pilote et sortie
audio en haut, **Audio / MIDI**, **Instruments et effets** (recherche des VST3, AU, LV2), le manuel
et le site. Double-clic sur un projet pour l'ouvrir. La case « Ouvrir le Hub au démarrage » le
désactive ; il reste dans **Fichier → Hub…**, et les récents dans **Fichier → Ouvrir un projet récent**.

## Les menus

| Menu | Contenu |
|---|---|
| Fichier | Hub, nouveau, ouvrir, projets récents, enregistrer (sous), importer un .mid dans la case, exporter le clip en .mid |
| Édition | annuler / rétablir (dans la fenêtre au premier plan : grille, éditeur de notes ou réglages de voie), renommer, dupliquer, copier, coller, supprimer le clip |
| Projet | ajouter une piste ou une scène, renommer ou supprimer la piste, supprimer la scène |
| MIDI | éditeur de notes, armer la piste, drum map GM, quantification du lancement, longueur d'enregistrement |
| Transport | lecture / stop, lancer la case ou la scène, tout arrêter, Ableton Link |
| Studio | Audio / MIDI, instruments et effets, réglages de la voie ou du master, console détaillée |
| Fenêtre, Aide | Hub, éditeur, fermer les fenêtres d'instruments ; mode d'emploi, site |

Dans la grille, **Cmd Z / Cmd Maj Z** annule et rétablit les modifications (clips, notes
enregistrées, noms, couleurs, déplacements).

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

**Console détaillée**, comme l'inspecteur de Cubase : le bouton « ▴ Console détaillée » sous le
master (ou F3) agrandit le bas de l'écran. Chaque piste y montre son panoramique, un **décalage**
de ± 100 ms (en retard, ou en avance pour rattraper un synthé externe lent), le **routage**
complet (entrée, canal, sortie, canal de sortie, retour audio) et la partie **MIDI** : changement
de **programme** (envoyé au lancement et à chaque changement), drum map GM, **transposition**
(± 48 demi-tons) et **vélocité** (± 127) appliquées en jouant, sans toucher aux notes des clips.
Double-clic sur une valeur pour la remettre à zéro.

**Voies et mixage** : chaque piste est une voie de console. Sa source est l'instrument virtuel,
ou le **retour audio** d'un synthé externe (une entrée de la carte son, choisie sous la sortie MIDI :
on traite alors son son sur la même piste qui le joue). Puis l'**égaliseur**, jusqu'à **8 inserts
d'effets** (VST3, AU, LV2), le panoramique et le volume, et enfin le **master** (égaliseur, inserts,
volume). Le bouton **e** ouvre les réglages de la voie, comme dans Cubase :

- **Inserts**, comme dans Cubase : 8 cases numérotées, et le son les traverse de 1 à 8 (une
  compression avant ou après la reverb ne sonne pas pareil). Clic sur n'importe quelle case vide
  (« + ») pour y mettre un effet ; **glisser** un effet sur une autre case pour changer sa place
  (vers une case vide, il y va ; sur un effet, il s'intercale et les suivants descendent). Clic sur
  le nom : ouvrir l'effet ; ⏻ : le contourner ; ▾ : le remplacer, le déplacer ou le retirer.
- **Égaliseur** à 4 bandes : le type de chaque bande se choisit avec des pictogrammes (1 : plateau
  grave, cloche ou coupe-bas ; 2 et 3 : cloche ou plateaux ; 4 : plateau aigu, cloche ou coupe-haut,
  coupes à 24 dB/octave). On tire les points de la courbe
  (fréquence et gain), molette sur un point pour la largeur, double-clic pour remettre à 0 dB ;
  ou les potards sous la courbe. « EQ » le coupe, « Réinitialiser » le remet à plat.
- **Fader** gradué en dB comme sur une voie de Cubase, vu-mètre gradué avec sa crête (clic pour
  la remettre à zéro), valeur du volume à taper, panoramique, M et S. Les vu-mètres sont aussi
  sous chaque piste de la console.
- En haut à droite : **← →** annuler / rétablir les réglages de la voie (aussi Cmd Z / Cmd Maj Z),
  **↑ ↓** passer à la piste précédente ou suivante (aussi les flèches du clavier).

Un second clic sur **e** referme la fenêtre.

Les entrées de la carte son ne s'ouvrent qu'au premier retour audio choisi (macOS demande alors
l'accès au micro). Muet et solo coupent aussi le son de la voie.

**Ableton Link** : le bouton **Link** à côté du tempo synchronise Grid Play avec Serato DJ,
Ableton Live, Traktor… sur le même ordinateur ou le réseau local : même tempo (le changer d'un côté
le change partout) et même mesure (en démarrant, Grid Play attend la prochaine mesure du groupe).
Le chiffre à côté de « Link » est le nombre d'autres participants. Dans Serato DJ, activer Link
dans les réglages : le bouton Sync des platines devient « LINK ». Le choix est gardé d'une fois sur l'autre.

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
| Cmd Z / Cmd Maj Z | annuler, rétablir |
| F3 | console détaillée / compacte |

(Ctrl à la place de Cmd sous Windows et Linux.) Les morceaux s'enregistrent en `.gridplay`
(XML lisible), avec l'état des instruments.

## Feuille de route

1. **Matrice de lancement** : faite.
2. **Éditeur de notes** façon éditeur MIDI de Cubase (interface et raccourcis) : fait.
3. **Consolidation** (notes du 20 novembre 2026) :
   - Fait : instruments virtuels et retours audio des synthés externes traités sur leur piste,
     inserts d'effets et égaliseur par piste et sur le master, panoramique, vu-mètres.
   - Fait : **Ableton Link** pour se synchroniser avec Serato DJ et les autres logiciels.
   - Fait : Hub de démarrage, menus complets, annuler / rétablir dans la grille et les voies,
     fader façon Cubase, console détaillée (routage, programme, transposition, vélocité, retard).
   - Abandonné (21 novembre 2026) : le mode DJ intégré. Les fonctions DJ seront plutôt confiées
     à un logiciel de DJ (Serato, ou Mixxx adapté), synchronisé par Link.

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
affiche la crête de sortie. `GRIDPLAY_TEST_CHANNEL=piste` : réglages de voie (`-voie.png`) ;
`GRIDPLAY_TEST_HUB=1` : le Hub (`-hub.png`) ; `GRIDPLAY_TEST_DETAILED=1` : console détaillée.

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
| `build/_deps/link-src` | Ableton Link 3.1.5 (GPLv2+), téléchargé par CMake |
| `Resources/Fonts/` | Police Source Sans 3 (Adobe, licence SIL OFL 1.1, voir `OFL.txt`), intégrée à l'appli |
| `Source/ChannelEq.h`, `ChannelEditor.*` | L'égaliseur de voie (testé) et la fenêtre des réglages de voie |
| `Source/Hub.*` | Le Hub de démarrage : projets récents, réglages audio, MIDI et VST |
| `Source/MainComponent.*`, `Main.cpp` | Fenêtre, transport, menus, raccourcis, fichiers |

Licence GNU AGPL v3.
