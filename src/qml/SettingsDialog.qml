import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

QQC2.Dialog {
    id: root

    title: "Configurações"
    modal: true
    width: Math.min(560, parent.width - 40)
    height: Math.min(implicitHeight, parent.height - 40)
    standardButtons: QQC2.Dialog.Save | QQC2.Dialog.Cancel

    onAboutToShow: {
        const settings = manager.settings;
        recordDirField.text = settings.recordDir;
        snapshotDirField.text = settings.snapshotDir;
        cooldownSpin.value = settings.alertCooldown;
        autoRecordSpin.value = settings.autoRecordSeconds;
        tcpBox.checked = settings.rtspTcp;
        lowLatencyBox.checked = settings.lowLatency;
        unfocusBox.checked = settings.pauseOnUnfocus;
        autostartBox.checked = manager.autostart;
    }
    onAccepted: {
        manager.setSetting("recordDir", recordDirField.text.trim());
        manager.setSetting("snapshotDir", snapshotDirField.text.trim());
        manager.setSetting("alertCooldown", cooldownSpin.value);
        manager.setSetting("autoRecordSeconds", autoRecordSpin.value);
        manager.setSetting("rtspTcp", tcpBox.checked);
        manager.setSetting("lowLatency", lowLatencyBox.checked);
        manager.setSetting("pauseOnUnfocus", unfocusBox.checked);
        manager.autostart = autostartBox.checked;
    }

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
                text: "Pasta das gravações"
            }
            QQC2.TextField {
                id: recordDirField

                Layout.fillWidth: true
            }

            QQC2.Label {
                text: "Pasta das capturas"
            }
            QQC2.TextField {
                id: snapshotDirField

                Layout.fillWidth: true
            }

            RowLayout {
                Layout.topMargin: 8

                QQC2.Label {
                    Layout.fillWidth: true
                    text: "Intervalo mínimo entre alertas da mesma câmera (s)"
                }
                QQC2.SpinBox {
                    id: cooldownSpin

                    from: 5
                    to: 3600
                    stepSize: 5
                }
            }
            RowLayout {
                QQC2.Label {
                    Layout.fillWidth: true
                    text: "Duração da gravação automática por alerta (s)"
                }
                QQC2.SpinBox {
                    id: autoRecordSpin

                    from: 5
                    to: 3600
                    stepSize: 5
                }
            }

            QQC2.CheckBox {
                id: autostartBox

                Layout.topMargin: 8
                text: "Iniciar em segundo plano ao fazer login"
            }
            QQC2.CheckBox {
                id: unfocusBox

                text: "Pausar as câmeras quando a janela perder o foco"
            }
            QQC2.CheckBox {
                id: lowLatencyBox

                text: "Modo de baixa latência"
            }
            QQC2.CheckBox {
                id: tcpBox

                text: "Usar RTSP sobre TCP (evita imagem borrada em redes instáveis)"
            }
        }
    }
}
