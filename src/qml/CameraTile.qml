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

    CameraPlayer {
        id: player

        anchors.fill: parent
        // Na grade o substream basta; em destaque usa o stream principal.
        source: !root.focused && root.camera.subUrl ? root.camera.subUrl : root.camera.url
        active: root.streamActive
        muted: root.muted
        lowLatency: manager.settings.lowLatency
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

    Rectangle {
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
                onClicked: manager.snapshotFromSink(root.camera.id, player.videoSink)
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
