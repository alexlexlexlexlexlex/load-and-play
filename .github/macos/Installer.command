#!/bin/bash
# Installe les plugins Load & Play de ce dossier sur ce Mac.
#
# - copie les VST3, AU, CLAP et LV2 dans ~/Library/Audio/Plug-Ins (pour cet utilisateur) ;
# - retire la « quarantaine » que macOS pose sur les fichiers téléchargés ou reçus par AirDrop
#   (sans quoi Cubase, Logic, Ableton... ne les chargent pas) ;
# - re-signe chaque plugin localement (signature « ad hoc », exigée sur les Mac Apple Silicon).
#
# Premier lancement : clic droit sur ce fichier > Ouvrir, puis « Ouvrir ».

cd "$(dirname "$0")" || exit 1
plugins="$HOME/Library/Audio/Plug-Ins"
count=0

install_bundle () {
    local source="$1" folder="$2"
    local name
    name=$(basename "$source")
    mkdir -p "$plugins/$folder"
    rm -rf "${plugins:?}/$folder/$name"
    ditto "$source" "$plugins/$folder/$name"
    xattr -dr com.apple.quarantine "$plugins/$folder/$name" 2>/dev/null
    if [ "$folder" = "LV2" ]; then
        # LV2 : un simple dossier, on signe la bibliothèque qu'il contient.
        for library in "$plugins/$folder/$name"/*.so "$plugins/$folder/$name"/*.dylib; do
            [ -e "$library" ] && codesign --force --sign - "$library" >/dev/null 2>&1
        done
    else
        codesign --force --deep --sign - "$plugins/$folder/$name" >/dev/null 2>&1
    fi
    echo "  ✓ $name  →  $plugins/$folder"
    count=$((count + 1))
}

echo "Installation des plugins Load & Play..."
echo
shopt -s nullglob
for b in VST3/*.vst3 *.vst3;             do install_bundle "$b" "VST3"; done
for b in AU/*.component *.component;     do install_bundle "$b" "Components"; done
for b in CLAP/*.clap *.clap;             do install_bundle "$b" "CLAP"; done
for b in LV2/*.lv2 *.lv2;                do install_bundle "$b" "LV2"; done

# Applications autonomes : elles restent dans ce dossier, on retire seulement la quarantaine.
for app in Standalone/*.app *.app; do
    xattr -dr com.apple.quarantine "$app" 2>/dev/null
    codesign --force --deep --sign - "$app" >/dev/null 2>&1
    echo "  ✓ $(basename "$app") prête à être ouverte"
done

# Les Audio Units installées apparaissent sans redémarrer le Mac.
killall -9 AudioComponentRegistrar >/dev/null 2>&1

echo
if [ "$count" -gt 0 ]; then
    echo "C'est installé ($count plugins). Relancez votre logiciel de musique."
    echo "Cubase : s'il ne voit pas encore le plugin, Studio > VST Plug-in Manager > « Rechercher à nouveau »,"
    echo "et réactivez-le s'il apparaît dans la liste des plugins bloqués."
else
    echo "Aucun plugin trouvé à côté de ce fichier."
fi
echo
read -n 1 -s -r -p "Appuyez sur une touche pour fermer."
echo
