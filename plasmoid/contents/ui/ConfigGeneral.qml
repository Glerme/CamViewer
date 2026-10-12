import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    id: page

    property string cfg_cameraId
    property alias cfg_muted: mutedBox.checked
    property alias cfg_useMainStream: mainStreamBox.checked
    property alias cfg_pauseWhenCovered: coveredBox.checked
    property bool cfg_pinned

    readonly property var choices: [{
        "id": "",
        "name": "Primeira câmera da lista"
    }].concat(backend.cameras)

    Backend {
        id: backend
    }

    Kirigami.FormLayout {
        QQC2.ComboBox {
            Kirigami.FormData.label: "Câmera:"
            model: page.choices
            textRole: "name"
            valueRole: "id"
            currentIndex: Math.max(0, page.choices.findIndex(choice => choice.id === page.cfg_cameraId))
            onActivated: page.cfg_cameraId = currentValue
        }

        QQC2.CheckBox {
            id: mutedBox

            Kirigami.FormData.label: "Áudio:"
            text: "Sem som"
        }

        QQC2.CheckBox {
            id: mainStreamBox

            Kirigami.FormData.label: "Qualidade:"
            text: "Usar o stream principal em vez do substream"
        }

        QQC2.CheckBox {
            id: coveredBox

            Kirigami.FormData.label: "Área de trabalho:"
            text: "Pausar quando uma janela maximizada cobrir o widget"
        }
    }
}
