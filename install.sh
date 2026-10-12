#!/bin/sh
# Compila e instala o CamViewer (app + widget do Plasma) só para o seu usuário.
#
#   ./install.sh               instala em ~/.local
#   ./install.sh --autostart   também inicia em segundo plano no login
#                              (para receber alertas sem abrir o app)
set -eu

cd "$(dirname "$0")"

PREFIX="${PREFIX:-$HOME/.local}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build build -j
cmake --install build

# O plasmashell lê esta variável no login; ela faz o vídeo do widget usar
# RTSP sobre TCP, como o app já faz.
mkdir -p "$CONFIG_HOME/plasma-workspace/env"
cp data/camviewer-env.sh "$CONFIG_HOME/plasma-workspace/env/camviewer.sh"

if [ "${1:-}" = "--autostart" ]; then
    mkdir -p "$CONFIG_HOME/autostart"
    sed "s|^Exec=.*|Exec=$PREFIX/bin/camviewer --background|" \
        build/org.camviewer.CamViewer.desktop > "$CONFIG_HOME/autostart/org.camviewer.CamViewer.desktop"
fi

echo
echo "Instalado. Abra \"CamViewer\" no menu de aplicativos e adicione o widget"
echo "\"CamViewer\" à área de trabalho ou ao painel."
echo "Se o widget já estava em uso, reinicie o Plasma: systemctl --user restart plasma-plasmashell"
