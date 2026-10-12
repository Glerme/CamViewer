// Player de uma câmera. Usado pelo app e pelo widget do Plasma.
//
// Enquanto `active` for falso não existe MediaPlayer nenhum: a conexão com a
// câmera, o decodificador e os buffers de vídeo são destruídos, então uma
// câmera pausada não gasta rede, CPU nem memória.

import QtQuick
import QtMultimedia

Item {
    id: root

    property string source
    property bool active: true
    property bool muted: true
    property bool lowLatency: true
    property int fillMode: VideoOutput.PreserveAspectFit

    readonly property var videoSink: loader.item ? loader.item.sink : null
    readonly property bool playing: loader.item ? loader.item.hasVideo : false
    property string errorText

    onSourceChanged: {
        errorText = "";
        retry.stop();
    }
    onActiveChanged: {
        if (!active) {
            errorText = "";
            retry.stop();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Loader {
        id: loader

        anchors.fill: parent
        active: root.active && root.source !== "" && !retry.running

        sourceComponent: Item {
            id: view

            readonly property alias sink: output.videoSink
            property bool hasVideo: false

            MediaPlayer {
                id: player

                videoOutput: output
                audioOutput: root.muted ? null : audio

                onErrorOccurred: (error, errorString) => {
                    root.errorText = errorString;
                    retry.restart();
                }
                onMediaStatusChanged: {
                    if (mediaStatus === MediaPlayer.EndOfMedia || mediaStatus === MediaPlayer.InvalidMedia) {
                        retry.restart();
                    }
                }

                Component.onCompleted: {
                    if (root.lowLatency) {
                        // Qt 6.10+: menos buffer e análise inicial mais curta.
                        try {
                            const options = player.playbackOptions;
                            options.playbackIntent = 1; // LowLatencyStreaming
                            player.playbackOptions = options;
                        } catch (e) {
                        }
                    }
                    player.source = root.source;
                    player.play();
                }
            }

            AudioOutput {
                id: audio
            }

            VideoOutput {
                id: output

                anchors.fill: parent
                fillMode: root.fillMode
            }

            Connections {
                target: output.videoSink

                function onVideoFrameChanged() {
                    if (!view.hasVideo) {
                        view.hasVideo = true;
                        root.errorText = "";
                    }
                }
            }
        }
    }

    // Enquanto roda, o Loader fica inativo; ao disparar, ele reconecta.
    Timer {
        id: retry

        interval: 5000
    }

    Text {
        anchors.centerIn: parent
        width: parent.width - 16
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: "#c0c0c0"
        visible: !root.playing
        text: {
            if (root.source === "") {
                return "Sem endereço de vídeo";
            }
            if (!root.active) {
                return "Pausado";
            }
            if (retry.running) {
                return (root.errorText || "Sem sinal") + "\nReconectando…";
            }
            return "Conectando…";
        }
    }
}
