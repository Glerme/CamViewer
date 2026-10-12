import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window

QQC2.ApplicationWindow {
    id: window

    // Câmera em destaque; vazio mostra a grade com todas.
    property string focusedCameraId

    readonly property var cameras: manager.cameras
    readonly property var visibleCameras: {
        const focused = cameras.filter(camera => camera.id === focusedCameraId);
        return focused.length > 0 ? focused : cameras;
    }
    // Janela minimizada (ou, se configurado, sem foco) não mantém streams.
    readonly property bool streamsActive: visibility !== Window.Minimized && visibility !== Window.Hidden
        && (!manager.settings.pauseOnUnfocus || active)

    title: "CamViewer"
    width: 1100
    height: 700
    minimumWidth: 480
    minimumHeight: 320
    visible: true

    header: QQC2.ToolBar {
        RowLayout {
            anchors.fill: parent

            QQC2.ToolButton {
                icon.name: "go-previous"
                text: "Todas as câmeras"
                visible: window.focusedCameraId !== "" && window.cameras.length > 1
                onClicked: window.focusedCameraId = ""
            }
            QQC2.ToolButton {
                icon.name: "list-add"
                text: "Adicionar câmera"
                onClicked: cameraDialog.openFor(null)
            }
            Item {
                Layout.fillWidth: true
            }
            QQC2.ToolButton {
                icon.name: "folder-videos"
                text: "Gravações"
                onClicked: manager.openFolder("record")
            }
            QQC2.ToolButton {
                icon.name: "folder-pictures"
                text: "Capturas"
                onClicked: manager.openFolder("snapshot")
            }
            QQC2.ToolButton {
                icon.name: "configure"
                text: "Configurações"
                onClicked: settingsDialog.open()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#101010"
    }

    GridLayout {
        id: grid

        anchors.fill: parent
        anchors.margins: 2
        columns: Math.max(1, Math.ceil(Math.sqrt(window.visibleCameras.length)))
        rowSpacing: 2
        columnSpacing: 2

        Repeater {
            model: window.visibleCameras

            CameraTile {
                required property var modelData

                Layout.fillWidth: true
                Layout.fillHeight: true
                // Divide o espaço igualmente, sem depender do tamanho do vídeo.
                Layout.preferredWidth: 1
                Layout.preferredHeight: 1

                camera: modelData
                streamActive: window.streamsActive
                focused: window.focusedCameraId === modelData.id
                onToggleFocus: window.focusedCameraId = focused ? "" : modelData.id
                onEditRequested: cameraDialog.openFor(modelData)
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        visible: window.cameras.length === 0
        spacing: 12

        QQC2.Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Nenhuma câmera cadastrada"
            color: "#d0d0d0"
            font.pointSize: 14
        }
        QQC2.Button {
            Layout.alignment: Qt.AlignHCenter
            icon.name: "list-add"
            text: "Adicionar câmera"
            onClicked: cameraDialog.openFor(null)
        }
    }

    CameraDialog {
        id: cameraDialog

        anchors.centerIn: parent
    }

    SettingsDialog {
        id: settingsDialog

        anchors.centerIn: parent
    }

    Shortcut {
        sequence: "Esc"
        enabled: window.focusedCameraId !== ""
        onActivated: window.focusedCameraId = ""
    }
    Shortcut {
        sequence: "F11"
        onActivated: window.visibility = window.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
    }
}
