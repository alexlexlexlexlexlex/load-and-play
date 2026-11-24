# Load & Play

Trois synthés virtuels et deux effets, gratuits et open source (AU, VST3, CLAP, LV2,
pour macOS, Windows et Linux), et le site qui les présente.

**Site : https://loadandplaymusic.com/** — on peut y jouer les
deux instruments dans le navigateur et télécharger les plugins.

| Dossier | Contenu |
|---|---|
| `LP60/` | **LP-60**, inspiré du Juno-60 et calé sur un Roland JU-06A : code source, tests, outils de mesure et de comparaison (`Tools/`) |
| `MiniMoog/` | **Model Play**, inspiré du Minimoog : code source et tests |
| `LP303/` | **LP-303** (nouveau), basse acide dans l'esprit de la TB-303, avec séquenceur 16 pas intégré : code source et tests |
| `DuckHunt/` | **Duck Hunt**, effet de ducking par sidechain, MIDI ou tempo, en pleine bande, 3 bandes ou spectral |
| `ChopChop/` | **Chop Chop**, repeater de voix distordu : séquenceur de 16 pas, drums en sidechain ou MIDI |
| `GridPlay/` | **Grid Play** (en construction), lanceur de boucles MIDI façon session d'Ableton, avec APC mini |
| `Site web/` | Le site (HTML, CSS, JavaScript), mis en ligne automatiquement sur GitHub Pages |
| `.github/workflows/` | Compilation des plugins sur les machines de GitHub, mise en ligne du site |

Les enregistrements du kit de mesures du LP-60 sont trop lourds pour le dépôt : ils sont
publiés dans les Releases (`LP-60-kit-mesures-JU-06A.zip`).

## Recompiler

Il faut CMake et Ninja (`pip3 install cmake ninja`) et un compilateur C++
(Command Line Tools d'Apple sur Mac).

```bash
cd LP60        # ou MiniMoog, LP303, DuckHunt, ChopChop, GridPlay
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

JUCE et l'extension CLAP sont téléchargés automatiquement par CMake. En local,
`LP60/JUCE` et `LP60/clap-juce-extensions` peuvent pointer vers ceux de `MiniMoog/`
pour éviter de les télécharger deux fois.

## Comparer LP-60 au JU-06A

Il faut Python 3 avec `numpy` et `scipy` (`pip3 install numpy scipy`), et le kit de
mesures dézippé à côté du dépôt :

```bash
cd LP60
python3 Tools/compare.py "../Kit mesures LP-60 (JU-06A)"
```

## Publier une nouvelle version

Voir `Site web/README.md`.

## Licence

GNU AGPL v3. Juno-60, JU-06A et TB-303 sont des marques de Roland Corporation ; Minimoog
et Model D sont des marques de Moog Music Inc. Elles ne sont citées que pour décrire
l'inspiration sonore : Load & Play n'est affilié à aucune de ces sociétés.
