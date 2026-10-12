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
        rotationBox.currentIndex = camera ? Math.max(0, rotationBox.indexOfValue(camera.rotation)) : 0;
        mirrorBox.checked = camera ? camera.mirror : false;
        ptzBox.checked = camera ? camera.ptz : false;
        onvifPortSpin.value = camera ? camera.onvifPort : 0;
        invertPanBox.checked = camera ? camera.ptzInvertPan : false;
        invertTiltBox.checked = camera ? camera.ptzInvertTilt : false;
        zoomBox.checked = camera ? camera.ptzZoom : false;
        open();
        nameField.forceActiveFocus();
    }

    title: cameraId ? "Editar câmera" : "Adicionar câmera"
    modal: true
    width: Math.min(560, parent.width - 40)
    // Em janelas baixas o formulário rola em vez de vazar para fora.
    height: Math.min(implicitHeight, parent.height - 40)
    standardButtons: QQC2.Dialog.Save | QQC2.Dialog.Cancel

    onAccepted: manager.saveCamera({
        id: cameraId,
        name: nameField.text,
        url: urlField.text,
        subUrl: subUrlField.text,
        motion: motionBox.checked,
        sensitivity: Math.round(sensitivitySlider.value),
        autoRecord: autoRecordBox.checked,
        rotation: rotationBox.currentValue,
        mirror: mirrorBox.checked,
        ptz: ptzBox.checked,
        onvifPort: onvifPortSpin.value,
        ptzInvertPan: invertPanBox.checked,
        ptzInvertTilt: invertTiltBox.checked,
        ptzZoom: zoomBox.checked
    })

    Component.onCompleted: standardButton(QQC2.Dialog.Save).enabled = Qt.binding(() => urlField.text.trim() !== "")

    contentItem: QQC2.ScrollView {
        id: scroll

        contentWidth: availableWidth
        clip: true
        leftPadding: 12
        rightPadding: 16
        topPadding: 8
        bottomPadding: 8

        ColumnLayout {
            width: scroll.availableWidth
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

            RowLayout {
                Layout.topMargin: 8

                QQC2.Label {
                    text: "Girar a imagem"
                }
                QQC2.ComboBox {
                    id: rotationBox

                    textRole: "text"
                    valueRole: "value"
                    model: [
                        {
                            text: "Não girar",
                            value: 0
                        },
                        {
                            text: "90° à direita",
                            value: 90
                        },
                        {
                            text: "180° (de cabeça para baixo)",
                            value: 180
                        },
                        {
                            text: "90° à esquerda",
                            value: 270
                        }
                    ]
                }
                QQC2.CheckBox {
                    id: mirrorBox

                    text: "Espelhar"
                }
            }

            QQC2.CheckBox {
                id: ptzBox

                Layout.topMargin: 8
                text: "Câmera com movimento (PTZ via ONVIF)"
            }
            RowLayout {
                Layout.leftMargin: 24
                enabled: ptzBox.checked

                QQC2.Label {
                    text: "Porta ONVIF"
                }
                QQC2.SpinBox {
                    id: onvifPortSpin

                    from: 0
                    to: 65535
                    editable: true
                    textFromValue: value => value === 0 ? "Automática" : String(value)
                    valueFromText: text => parseInt(text) || 0
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.7
                    text: "Deixe em Automática (0) para o app procurar. Usa o usuário e a senha do endereço do vídeo."
                }
            }

            QQC2.CheckBox {
                id: invertPanBox

                Layout.leftMargin: 24
                enabled: ptzBox.checked
                text: "Inverter esquerda/direita (se as setas andarem ao contrário)"
            }
            QQC2.CheckBox {
                id: invertTiltBox

                Layout.leftMargin: 24
                enabled: ptzBox.checked
                text: "Inverter cima/baixo"
            }
            QQC2.CheckBox {
                id: zoomBox

                Layout.leftMargin: 24
                enabled: ptzBox.checked
                text: "A câmera tem zoom motorizado (mostra os botões + e −)"
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
}
