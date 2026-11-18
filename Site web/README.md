# Site Load & Play

Site statique (HTML, CSS, JavaScript, sans outil de construction) qui présente
le LP-60 et Model Play, avec les instruments jouables dans le navigateur.

En ligne : https://alexlexlexlexlexlex.github.io/load-and-play/

## Voir le site en local

Les instruments jouables ont besoin d'un petit serveur (les navigateurs
refusent l'audio en `file://`) :

```bash
cd "Site web" && python3 -m http.server 8765
```

puis ouvrir http://localhost:8765.

## Réglages : `assets/js/config.js`

| Réglage | Rôle |
| --- | --- |
| `email` | adresse du bouton « Contact » et du lien de la page *what about the law?* (vide = bouton masqué) |
| `github.user` | pseudo GitHub : lien « GitHub » et adresse des téléchargements (vide = boutons grisés) |
| `github.repo` | nom du dépôt, `load-and-play` |
| `versions` | numéros de version utilisés dans les noms des fichiers à télécharger |

## Mise en ligne

Tout le dossier « Synthés Load and Play » est le dépôt GitHub `load-and-play`.

- **Le site** se met en ligne tout seul à chaque `git push` qui touche « Site web »
  (`.github/workflows/site.yml`, GitHub Pages en mode « GitHub Actions »).
- **Les plugins** sont compilés sur les machines de GitHub pour macOS, Windows,
  Linux x64 et Linux ARM64 à chaque envoi (`.github/workflows/plugins.yml`).

## Publier une nouvelle version

1. Changer `versions` dans `assets/js/config.js` (et la version dans les `CMakeLists.txt`).
2. Envoyer, puis poser l'étiquette de version :
   ```bash
   git commit -am "Version 1.1.0" && git push
   git tag v1.1.0 && git push origin v1.1.0
   ```
3. GitHub compile tout et range dans la Release `v1.1.0` les zips des plugins
   et du code source, avec les noms qu'attend le site.
4. Ajouter à la main le kit de mesures, qui n'est pas dans le dépôt :
   ```bash
   gh release upload v1.1.0 "Publication 1.0.0/LP-60-kit-mesures-JU-06A.zip"
   ```

Les boutons visent toujours la **dernière** Release
(`…/releases/latest/download/<fichier>`) : chaque Release doit contenir tous les fichiers.

| Fichier | Contenu |
| --- | --- |
| `LP-60-<version>-macOS.zip` | AU, VST3, CLAP, LV2 et appli autonome (Mac Intel et Apple Silicon) |
| `LP-60-<version>-Windows.zip` | VST3, CLAP, LV2 et appli autonome |
| `LP-60-<version>-Linux-x64.zip` | VST3, CLAP, LV2 et appli autonome |
| `LP-60-<version>-Linux-ARM64.zip` | idem pour Raspberry Pi et autres ARM 64 bits |
| `LP-60-<version>-source.zip` | le dossier `LP60/` |
| `LP-60-kit-mesures-JU-06A.zip` | enregistrements du JU-06A, fichiers MIDI, rendus et rapport de comparaison |
| `Model-Play-<version>-…` | les mêmes, pour Model Play (sans kit de mesures) |

## Outils (`tools/`, non publiés sur le site)

- `render_web.mjs` et `compare_web.py` vérifient que les moteurs JavaScript
  (`assets/js/dsp/`) sonnent comme les plugins : même niveau, spectre à
  ~0,2 dB près.
  ```bash
  python tools/compare_web.py "<dossier Synthés Load and Play>" <dossier temporaire>
  ```
  Il faut d'abord compiler `RenderTest` dans `LP60/build` et `MiniMoog/build`.
