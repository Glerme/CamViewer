#!/bin/sh
# Remove o que o install.sh instalou. Gravações, capturas e a lista de
# câmeras (~/.config/camviewer) são mantidas.
set -u

PREFIX="${PREFIX:-$HOME/.local}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"

busctl --user call org.camviewer.CamViewer /CamViewer org.camviewer.CamViewer Quit 2>/dev/null

kpackagetool6 --type Plasma/Applet --remove org.camviewer.plasmoid
rm -f "$PREFIX/bin/camviewer" \
      "$PREFIX/share/applications/org.camviewer.CamViewer.desktop" \
      "$PREFIX/share/dbus-1/services/org.camviewer.CamViewer.service" \
      "$PREFIX/share/knotifications6/camviewer.notifyrc" \
      "$CONFIG_HOME/plasma-workspace/env/camviewer.sh" \
      "$CONFIG_HOME/autostart/org.camviewer.CamViewer.desktop"
