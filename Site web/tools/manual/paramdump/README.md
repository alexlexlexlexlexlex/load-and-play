# ParamDump

Petit hôte JUCE qui charge un VST3 et affiche ses réglages (nom, plage, valeur par
défaut, choix) en JSON, et ses presets sur la sortie d'erreur. Sert à remplir
`../params.json` pour les manuels.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # JUCE : ../../../../MiniMoog/JUCE
cmake --build build
./build/ParamDump_artefacts/Release/ParamDump ~/Library/Audio/Plug-Ins/VST3/"Duck Hunt.vst3"
```
