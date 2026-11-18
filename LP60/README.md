# LP-60 — Load & Play

Synthé polyphonique 6 voix inspiré du Roland Juno-60. AU, VST3, CLAP, LV2 et
application autonome, pour macOS (Apple Silicon + Intel, 10.13+), Windows et Linux.

## Compiler

CMake 3.28+ et un compilateur C++17 (Command Line Tools d'Apple, Visual Studio 2022, GCC/Clang).
JUCE et l'extension CLAP sont téléchargés automatiquement s'ils ne sont pas dans le dossier.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/RenderTest_artefacts/Release/RenderTest demo.wav   # contrôles automatiques + démo
```

Windows et Linux (x64, ARM64) : compilation automatique sur GitHub, `../.github/workflows/plugins.yml`.

## Le son, et d'où viennent les valeurs

Sources : notes de service Roland Juno-60 (1983) et mesures de
[pendragon-andyh](https://github.com/pendragon-andyh/Juno60).

| Section | Comportement |
|---|---|
| Voix | 6, attribuées en rotation comme le mode ROTARY du Juno-60 ; légères différences entre voix |
| DCO | Dent de scie et pulse cumulables, sous-oscillateur carré à l'octave inférieure, bruit filtré à 5 kHz ; ranges 16', 8', 4' ; hauteur stable (horloge numérique) |
| PWM | 50 % à 95 % de rapport cyclique, par LFO, manuel ou enveloppe |
| LFO | Triangle, 0,04 à 67 Hz, retard (attente puis montée), modes AUTO et MANUAL (molette) ; jusqu'à ±380 cents sur le DCO |
| HPF | 4 positions : aucun, 244 Hz, 579 Hz, 1 kHz (6 dB/oct), mesurées sur le JU-06A |
| VCF | 24 dB/oct, 4 étages saturés, résonance jusqu'à l'auto-oscillation (accordée à 2 % près), enveloppe ±, LFO, suivi de clavier 0-100 % |
| VCA | ENV, ou GATE (montée 3 ms, descente 6 ms, mesurées) |
| ENV | Une seule ADSR pour VCF et VCA ; attaque 1 ms à 3,25 s, decay et release 2 ms à 19,8 s, courbes mesurées |
| Chorus | I (0,55 Hz), II (0,89 Hz), I+II (7,6 Hz), stéréo dans les trois modes |
| Arpège | UP, U&D, DOWN, 1 à 3 octaves, 1,5 à 50 pas/s ou calé sur le tempo ; HOLD et pédale de sustain |

Le chorus reproduit aussi le souffle des lignes à retard BBD : sombre, présent dès que le
chorus est allumé, même sans jouer, réglable avec NOISE (5 = -76 dBFS par canal comme le JU-06A, 0 = coupé).

Encore estimé : les filtres et la saturation des lignes à retard du chorus, le centre de
leur retard (3,5 ms), et l'arpégiateur (pas encore comparé au JU-06A).

## Calage sur un Roland JU-06A

DCO, filtre, enveloppes et passe-haut sont calés sur des enregistrements d'un JU-06A
(mode Juno-60), avec `Tools/compare.py` : chaque test du kit de mesures
(`Tools/kit_tests.py`) est rejoué dans LP-60 et mesuré de la même façon.

- DCO : impulsion en phase avec la dent de scie, largeurs 49 % / 29 % / 4 %, volume constant avec le PWM.
- Filtre : coupure 190 Hz à 4, 1,2 kHz à 5, 7,7 kHz à 6, grand ouvert dès 8 ; résonance et perte de graves mesurées.
- Enveloppes : attaque presque linéaire (3 s au maximum), decay/release exponentiels (2,3 dB/s au maximum),
  sustain non linéaire, enveloppe vers le filtre jusqu'à 10,6 octaves.
- Passe-haut : 244 Hz, 579 Hz, 1 kHz.
- LFO : 0,042 Hz à 0, 4,2 Hz à 5, 67 Hz à 10 ; ±380 cents ; retard attente + montée.
- Chorus : vitesses et excursion du retard mesurées en suivant le retard au fil du temps ; souffle à -76 dBFS.

```bash
python3 Tools/compare.py "<dossier du kit>"      # rapport dans <kit>/Comparaison/Rapport.txt
```

## Licence

Copyright © 2026 Load & Play. GNU AGPL v3 (voir `LICENSE`). Juno-60 est une marque de
Roland Corporation, citée uniquement pour décrire l'inspiration sonore.
