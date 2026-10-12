import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid

PlasmoidItem {
    id: root

    readonly property bool inPanel: Plasmoid.formFactor === PlasmaCore.Types.Horizontal
        || Plasmoid.formFactor === PlasmaCore.Types.Vertical

    readonly property var camera: {
        const cameras = backend.cameras;
        return cameras.find(camera => camera.id === Plasmoid.configuration.cameraId) ?? cameras[0] ?? null;
    }
    readonly property bool recording: camera?.recording ?? false
    readonly property string streamUrl: {
        if (!camera) {
            return "";
        }
        return !Plasmoid.configuration.useMainStream && camera.subUrl ? camera.subUrl : camera.url;
    }
    // No painel só existe vídeo com o miniplayer aberto; na área de trabalho
    // ele para enquanto uma janela maximizada estiver por cima.
    readonly property bool streamActive: inPanel ? expanded : !(windows.item?.covered ?? false)

    function snapshot() {
        if (camera) {
            backend.call("Snapshot", [camera.id]);
        }
    }
    function toggleRecording() {
        if (camera) {
            backend.call("ToggleRecording", [camera.id]);
        }
    }
    function openApp() {
        backend.call("ShowWindow", [camera?.id ?? ""]);
        expanded = false;
    }

    preferredRepresentation: inPanel ? compactRepresentation : fullRepresentation
    hideOnWindowDeactivate: !Plasmoid.configuration.pinned
    toolTipMainText: "CamViewer"
    toolTipSubText: camera ? camera.name + (recording ? " — gravando" : "") : "Nenhuma câmera"

    Plasmoid.icon: "camera-video"
    Plasmoid.backgroundHints: PlasmaCore.Types.DefaultBackground | PlasmaCore.Types.ConfigurableBackground
    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: "Tirar captura"
            icon.name: "camera-photo"
            enabled: root.camera !== null
            onTriggered: root.snapshot()
        },
        PlasmaCore.Action {
            text: root.recording ? "Parar gravação" : "Gravar"
            icon.name: root.recording ? "media-playback-stop" : "media-record"
            enabled: root.camera !== null
            onTriggered: root.toggleRecording()
        },
        PlasmaCore.Action {
            text: "Abrir CamViewer"
            icon.name: "window-new"
            onTriggered: root.openApp()
        }
    ]

    onExpandedChanged: isExpanded => {
        if (isExpanded) {
            backend.refresh();
        }
    }

    Backend {
        id: backend
    }

    Loader {
        id: windows

        active: !root.inPanel && Plasmoid.configuration.pauseWhenCovered
        source: "WindowWatcher.qml"
        onLoaded: item.screenGeometry = Qt.binding(() => {
            // No login o containment ainda não tem tela, e ganhar uma não
            // dispara screenGeometryChanged: sem reler aqui, o filtro por
            // tela ficaria vazio e janelas de outro monitor pausariam o vídeo.
            Plasmoid.containment.screen;
            return Plasmoid.containment.screenGeometry;
        })
    }

    compactRepresentation: MouseArea {
        hoverEnabled: true
        onClicked: root.expanded = !root.expanded

        Kirigami.Icon {
            anchors.fill: parent
            source: "camera-video"
            active: parent.containsMouse
        }
        Rectangle {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: Math.max(6, parent.height / 3)
            height: width
            radius: width / 2
            color: Kirigami.Theme.negativeTextColor
            visible: root.recording
        }
    }

    fullRepresentation: Item {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 12
        Layout.minimumHeight: Kirigami.Units.gridUnit * 7
        Layout.preferredWidth: Kirigami.Units.gridUnit * 28
        Layout.preferredHeight: Kirigami.Units.gridUnit * 16

        CameraPlayer {
            id: player

            anchors.fill: parent
            visible: root.camera !== null
            source: root.streamUrl
            active: root.streamActive
            muted: Plasmoid.configuration.muted
            lowLatency: backend.lowLatency
            videoRotation: root.camera?.rotation ?? 0
            mirror: root.camera?.mirror ?? false
        }

        PlasmaExtras.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 2
            visible: root.camera === null
            iconName: "camera-video"
            text: backend.available ? "Nenhuma câmera cadastrada" : "CamViewer não está respondendo"
            explanation: backend.available ? "Adicione uma câmera no app CamViewer." : "Verifique se o app está instalado."
            helpfulAction: Kirigami.Action {
                text: backend.available ? "Abrir CamViewer" : "Tentar de novo"
                icon.name: backend.available ? "window-new" : "view-refresh"
                onTriggered: backend.available ? root.openApp() : backend.refresh()
            }
        }

        HoverHandler {
            id: hover
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: Kirigami.Units.smallSpacing
            width: badge.implicitWidth + Kirigami.Units.largeSpacing
            height: badge.implicitHeight + Kirigami.Units.smallSpacing
            radius: Kirigami.Units.cornerRadius
            color: "#b0000000"
            visible: root.camera !== null && (hover.hovered || root.recording)

            RowLayout {
                id: badge

                anchors.centerIn: parent
                spacing: Kirigami.Units.smallSpacing

                Rectangle {
                    width: Kirigami.Units.smallSpacing * 2
                    height: width
                    radius: width / 2
                    color: "#e53935"
                    visible: root.recording
                }
                PlasmaComponents.Label {
                    color: "white"
                    text: root.camera?.name ?? ""
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: controls.implicitHeight + Kirigami.Units.smallSpacing
            color: "#b0000000"
            visible: root.camera !== null && (hover.hovered || cameraBox.popup.visible)

            // As cores fixas do fundo pedem ícones claros em qualquer tema.
            Kirigami.Theme.inherit: false
            Kirigami.Theme.colorSet: Kirigami.Theme.Complementary

            RowLayout {
                id: controls

                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.smallSpacing
                anchors.rightMargin: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.ComboBox {
                    id: cameraBox

                    Layout.maximumWidth: Kirigami.Units.gridUnit * 10
                    visible: backend.cameras.length > 1
                    model: backend.cameras
                    textRole: "name"
                    valueRole: "id"
                    currentIndex: root.camera ? backend.cameras.findIndex(camera => camera.id === root.camera.id) : -1
                    onActivated: Plasmoid.configuration.cameraId = currentValue
                }
                Item {
                    Layout.fillWidth: true
                }
                PlasmaComponents.ToolButton {
                    icon.name: "camera-photo"
                    onClicked: root.snapshot()
                    PlasmaComponents.ToolTip.text: "Tirar captura"
                    PlasmaComponents.ToolTip.visible: hovered
                }
                PlasmaComponents.ToolButton {
                    icon.name: root.recording ? "media-playback-stop" : "media-record"
                    onClicked: root.toggleRecording()
                    PlasmaComponents.ToolTip.text: root.recording ? "Parar gravação" : "Gravar"
                    PlasmaComponents.ToolTip.visible: hovered
                }
                PlasmaComponents.ToolButton {
                    icon.name: Plasmoid.configuration.muted ? "audio-volume-muted" : "audio-volume-high"
                    onClicked: Plasmoid.configuration.muted = !Plasmoid.configuration.muted
                    PlasmaComponents.ToolTip.text: Plasmoid.configuration.muted ? "Ativar som" : "Silenciar"
                    PlasmaComponents.ToolTip.visible: hovered
                }
                PlasmaComponents.ToolButton {
                    visible: root.inPanel
                    icon.name: "window-pin"
                    checkable: true
                    checked: Plasmoid.configuration.pinned
                    onToggled: Plasmoid.configuration.pinned = checked
                    PlasmaComponents.ToolTip.text: "Manter aberto"
                    PlasmaComponents.ToolTip.visible: hovered
                }
                PlasmaComponents.ToolButton {
                    icon.name: "window-new"
                    onClicked: root.openApp()
                    PlasmaComponents.ToolTip.text: "Abrir CamViewer"
                    PlasmaComponents.ToolTip.visible: hovered
                }
            }
        }
    }
}
