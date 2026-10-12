import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

QQC2.Dialog {
    id: root

    property string cameraId

    function openFor(camera) {
        cameraId = camera ? camera.id : "";
        nameField.text = camera ? camera.name : "";
        urlField.text = camera ? camera.url : "";
        subUrlField.text = camera ? camera.subUrl : "";
        motionBox.checked = camera ? camera.motion : false;
        sensitivitySlider.value = camera ? camera.sensitivity : 5;
        autoRecordBox.checked = camera ? camera.autoRecord : false;
        open();
        nameField.forceActiveFocus();
    }

    title: cameraId ? "Editar câmera" : "Adicionar câmera"
    modal: true
    width: Math.min(560, parent.width - 40)
    standardButtons: QQC2.Dialog.Save | QQC2.Dialog.Cancel

    onAccepted: manager.saveCamera({
        id: cameraId,
        name: nameField.text,
        url: urlField.text,
        subUrl: subUrlField.text,
        motion: motionBox.checked,
        sensitivity: Math.round(sensitivitySlider.value),
        autoRecord: autoRecordBox.checked
    })

    Component.onCompleted: standardButton(QQC2.Dialog.Save).enabled = Qt.binding(() => urlField.text.trim() !== "")

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        QQC2.Label {
            text: "Nome"
        }
        QQC2.TextField {
            id: nameField

            Layout.fillWidth: true
            placeholderText: "Portão, Garagem…"
        }

        QQC2.Label {
            text: "Endereço do vídeo"
        }
        QQC2.TextField {
            id: urlField

            Layout.fillWidth: true
            placeholderText: "rtsp://usuario:senha@192.168.0.10:554/stream1"
        }

        QQC2.Label {
            text: "Endereço do substream (opcional)"
        }
        QQC2.TextField {
            id: subUrlField

            Layout.fillWidth: true
            placeholderText: "rtsp://usuario:senha@192.168.0.10:554/stream2"
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            opacity: 0.7
            font.pointSize: nameField.font.pointSize * 0.9
            text: "O substream (baixa resolução) é usado na grade, no widget e na detecção de movimento, para gastar menos memória e CPU. Gravações e capturas usam sempre o endereço principal."
        }

        QQC2.CheckBox {
            id: motionBox

            Layout.topMargin: 8
            text: "Notificar quando houver movimento"
        }
        RowLayout {
            Layout.leftMargin: 24
            enabled: motionBox.checked

            QQC2.Label {
                text: "Sensibilidade"
            }
            QQC2.Slider {
                id: sensitivitySlider

                Layout.fillWidth: true
                from: 1
                to: 10
                stepSize: 1
                snapMode: QQC2.Slider.SnapAlways
            }
            QQC2.Label {
                text: Math.round(sensitivitySlider.value)
            }
        }
        QQC2.CheckBox {
            id: autoRecordBox

            Layout.leftMargin: 24
            enabled: motionBox.checked
            text: "Gravar automaticamente ao detectar movimento"
        }

        QQC2.Button {
            Layout.topMargin: 8
            visible: root.cameraId !== ""
            icon.name: "edit-delete"
            text: "Remover câmera"
            onClicked: {
                manager.removeCamera(root.cameraId);
                root.close();
            }
        }
    }
}
