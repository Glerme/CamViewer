import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

Item {
    id: root

    required property var camera
    property bool streamActive: true
    property bool focused: false
    property bool muted: true

    readonly property var state: manager.runtime[camera.id] ?? ({})
    readonly property bool recording: state.recording ?? false

    signal toggleFocus
    signal editRequested

    function formatElapsed(since) {
        const total = Math.max(0, Math.floor((Date.now() - since) / 1000));
        const pad = n => String(n).padStart(2, "0");
        return pad(Math.floor(total / 3600)) + ":" + pad(Math.floor(total / 60) % 60) + ":" + pad(total % 60);
    }

    readonly property bool hasSubstream: camera.subUrl !== ""
    // Quadro mais recente em exibição, para a captura instantânea.
    readonly property var videoSink: mainPlayer.playing ? mainPlayer.videoSink : gridPlayer.videoSink

    onVisibleChanged: visible ? hiddenGrace.stop() : hiddenGrace.restart()
    onFocusedChanged: {
        if (!focused && hasSubstream) {
            mainLinger.restart();
        }
    }

    // Player da grade: o substream quando existe, senão o stream principal.
    // Continua tocando por um tempo depois de a câmera ser escondida (outra
    // foi destacada), para voltar à grade sem reconectar.
    CameraPlayer {
        id: gridPlayer

        anchors.fill: parent
        source: root.hasSubstream ? root.camera.subUrl : root.camera.url
        active: root.streamActive && (root.visible || hiddenGrace.running)
            && !(root.focused && mainPlayer.playing)
        muted: root.muted || mainPlayer.playing
        lowLatency: manager.settings.lowLatency
        videoRotation: root.camera.rotation
        mirror: root.camera.mirror
    }

    // Em destaque, o stream principal entra por cima quando já tem imagem, e
    // na volta só sai quando o da grade retomou: a troca não mostra tela preta.
    CameraPlayer {
        id: mainPlayer

        anchors.fill: parent
        visible: playing
        source: root.camera.url
        active: root.streamActive && root.hasSubstream && (root.focused || mainLinger.running)
        muted: root.muted
        lowLatency: manager.settings.lowLatency
        videoRotation: root.camera.rotation
        mirror: root.camera.mirror
    }

    Timer {
        id: hiddenGrace

        interval: 60000
    }
    Timer {
        id: mainLinger

        interval: 10000
    }
    Connections {
        target: gridPlayer

        function onPlayingChanged() {
            if (gridPlayer.playing) {
                mainLinger.stop();
            }
        }
    }

    HoverHandler {
        id: hover
    }
    TapHandler {
        onDoubleTapped: root.toggleFocus()
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        width: badge.implicitWidth + 12
        height: badge.implicitHeight + 6
        radius: 4
        color: "#b0000000"

        RowLayout {
            id: badge

            anchors.centerIn: parent
            spacing: 6

            Rectangle {
                visible: root.recording
                width: 10
                height: 10
                radius: 5
                color: "#e53935"

                SequentialAnimation on opacity {
                    running: root.recording
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 700 }
                    NumberAnimation { to: 1; duration: 700 }
                }
            }
            QQC2.Label {
                color: "white"
                text: root.camera.name + (root.recording ? "  " + elapsed.text : "")
            }
            QQC2.Label {
                visible: root.camera.motion
                color: "#ffca28"
                text: "●"
                QQC2.ToolTip.visible: alertHover.hovered
                QQC2.ToolTip.text: "Alertas de movimento ligados"

                HoverHandler {
                    id: alertHover
                }
            }
        }
    }

    // Só para recalcular o tempo de gravação a cada segundo.
    Timer {
        id: elapsed

        property string text

        interval: 1000
        repeat: true
        running: root.recording
        triggeredOnStart: true
        onTriggered: text = root.formatElapsed(root.state.since)
    }

    // Controle de movimento: a câmera anda enquanto o botão está apertado.
    component PtzButton: QQC2.RoundButton {
        property real pan: 0
        property real tilt: 0
        property real zoom: 0

        implicitWidth: 34
        implicitHeight: 34
        font.pixelSize: 15
        palette.buttonText: "white"
        background: Rectangle {
            radius: width / 2
            color: parent.down ? "#d0ffffff" : parent.hovered ? "#60ffffff" : "#30ffffff"
        }
        onPressedChanged: {
            if (pressed) {
                manager.PtzMove(root.camera.id, pan, tilt, zoom);
            } else {
                manager.PtzStop(root.camera.id);
            }
        }
    }

    Rectangle {
        id: ptzPad

        anchors.right: parent.right
        anchors.bottom: controlBar.top
        anchors.margins: 8
        width: ptzLayout.implicitWidth + 12
        height: ptzLayout.implicitHeight + 12
        radius: 8
        color: "#b0000000"
        visible: root.camera.ptz && hover.hovered

        RowLayout {
            id: ptzLayout

            anchors.centerIn: parent
            spacing: 10

            GridLayout {
                columns: 3
                rowSpacing: 2
                columnSpacing: 2

                Item { implicitWidth: 34; implicitHeight: 34 }
                PtzButton { text: "▲"; tilt: 0.5 }
                Item { implicitWidth: 34; implicitHeight: 34 }
                PtzButton { text: "◀"; pan: -0.5 }
                Item { implicitWidth: 34; implicitHeight: 34 }
                PtzButton { text: "▶"; pan: 0.5 }
                Item { implicitWidth: 34; implicitHeight: 34 }
                PtzButton { text: "▼"; tilt: -0.5 }
                Item { implicitWidth: 34; implicitHeight: 34 }
            }
            ColumnLayout {
                visible: root.camera.ptzZoom
                spacing: 2

                PtzButton { text: "+"; zoom: 0.5 }
                PtzButton { text: "−"; zoom: -0.5 }
            }
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.margins: 8
        width: Math.min(parent.width - 16, ptzError.implicitWidth + 16)
        height: ptzError.implicitHeight + 8
        radius: 4
        color: "#d0b71c1c"
        visible: ptzErrorTimer.running

        QQC2.Label {
            id: ptzError

            anchors.fill: parent
            anchors.margins: 4
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: "white"
        }
        Timer {
            id: ptzErrorTimer

            interval: 6000
        }
        Connections {
            target: manager

            function onPtzFailed(id, message) {
                if (id === root.camera.id) {
                    ptzError.text = "Não foi possível mover a câmera: " + message;
                    ptzErrorTimer.restart();
                }
            }
        }
    }

    Rectangle {
        id: controlBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: controls.implicitHeight + 8
        color: "#b0000000"
        opacity: hover.hovered ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }

        RowLayout {
            id: controls

            anchors.centerIn: parent
            spacing: 4

            QQC2.ToolButton {
                icon.name: "camera-photo"
                icon.color: "white"
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: "Tirar captura"
                onClicked: manager.snapshotFromSink(root.camera.id, root.videoSink)
            }
            QQC2.ToolButton {
                icon.name: root.recording ? "media-playback-stop" : "media-record"
                icon.color: root.recording ? "white" : "#e53935"
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: root.recording ? "Parar gravação" : "Gravar"
                onClicked: manager.ToggleRecording(root.camera.id)
            }
            QQC2.ToolButton {
                icon.name: root.muted ? "audio-volume-muted" : "audio-volume-high"
                icon.color: "white"
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: root.muted ? "Ativar som" : "Silenciar"
                onClicked: root.muted = !root.muted
            }
            QQC2.ToolButton {
                icon.name: root.focused ? "view-grid" : "view-fullscreen"
                icon.color: "white"
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: root.focused ? "Voltar para a grade" : "Destacar esta câmera"
                onClicked: root.toggleFocus()
            }
            QQC2.ToolButton {
                icon.name: "document-edit"
                icon.color: "white"
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: "Editar câmera"
                onClicked: root.editRequested()
            }
        }
    }
}
