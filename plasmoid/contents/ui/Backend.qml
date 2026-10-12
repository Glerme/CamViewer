// Conversa com o app CamViewer pelo D-Bus. A primeira chamada já inicia o
// app em segundo plano (ativação D-Bus) se ele não estiver rodando.

import QtQuick
import org.kde.plasma.workspace.dbus as DBus

Item {
    id: root

    readonly property string service: "org.camviewer.CamViewer"
    readonly property string path: "/CamViewer"

    property var cameras: []
    property bool lowLatency: true
    // Falso enquanto o app não respondeu (por exemplo, não instalado).
    property bool available: false

    property string lastState
    property DBus.dbusMessage message

    function call(member, args, callback) {
        message = {
            "service": service,
            "path": path,
            "iface": service,
            "member": member,
            "arguments": args,
            // Todos os métodos que o widget chama recebem só textos.
            "inSignature": args.length > 0 ? "s".repeat(args.length) : null
        };
        const reply = DBus.SessionBus.asyncCall(message);
        reply.finished.connect(() => {
            if (callback) {
                callback(reply.isError ? null : reply.value);
            }
            // Sem isto a resposta fica presa pela conexão do sinal.
            reply.destroy();
        });
    }

    function refresh() {
        call("State", [], value => {
            available = value !== null;
            // Só troca a lista quando algo mudou, para não reiniciar o vídeo.
            if (value === null || value === lastState) {
                return;
            }
            lastState = value;
            const state = JSON.parse(value);
            lowLatency = state.lowLatency;
            cameras = state.cameras;
        });
    }

    Component.onCompleted: refresh()

    DBus.SignalWatcher {
        busType: DBus.BusType.Session
        service: root.service
        path: root.path
        iface: root.service

        function dbusStateChanged() {
            root.refresh();
        }
    }
}
