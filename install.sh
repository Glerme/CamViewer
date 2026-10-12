#!/bin/sh
# Compila e instala o CamViewer (app + widget do Plasma) só para o seu usuário.
#
#   ./install.sh               instala em ~/.local
#   ./install.sh --autostart   também inicia em segundo plano no login
#                              (para receber alertas sem abrir o app)
set -eu

cd "$(dirname "$0")"

PREFIX="${PREFIX:-$HOME/.local}"
DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
PLASMOID_ID=org.camviewer.plasmoid

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build build -j
cmake --install build

# O pacote do widget precisa ser autossuficiente: leva uma cópia do player.
STAGE=build/plasmoid
rm -rf "$STAGE"
cp -r plasmoid "$STAGE"
cp shared/CameraPlayer.qml "$STAGE/contents/ui/"

if [ -d "$DATA_HOME/plasma/plasmoids/$PLASMOID_ID" ]; then
    kpackagetool6 --type Plasma/Applet --upgrade "$STAGE"
else
    kpackagetool6 --type Plasma/Applet --install "$STAGE"
fi

# O plasmashell lê esta variável no login; ela faz o vídeo do widget usar
# RTSP sobre TCP, como o app já faz.
mkdir -p "$CONFIG_HOME/plasma-workspace/env"
cat > "$CONFIG_HOME/plasma-workspace/env/camviewer.sh" <<'EOF'
export QT_FFMPEG_RTSP_TRANSPORT=tcp
EOF

if [ "${1:-}" = "--autostart" ]; then
    mkdir -p "$CONFIG_HOME/autostart"
    sed "s|^Exec=.*|Exec=$PREFIX/bin/camviewer --background|" \
        build/org.camviewer.CamViewer.desktop > "$CONFIG_HOME/autostart/org.camviewer.CamViewer.desktop"
fi

echo
echo "Instalado. Abra \"CamViewer\" no menu de aplicativos e adicione o widget"
echo "\"CamViewer\" à área de trabalho ou ao painel."
echo "Se o widget já estava em uso, reinicie o Plasma: systemctl --user restart plasma-plasmashell"
