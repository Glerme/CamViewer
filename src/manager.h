#pragma once

#include <QDateTime>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class MotionDetector;
class OnvifPtz;

// Guarda as câmeras e configurações, grava, tira capturas e dispara alertas.
// Fica vivo em segundo plano mesmo com a janela fechada; o widget do Plasma
// fala com ele pela interface D-Bus (os membros Q_SCRIPTABLE).
class Manager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.camviewer.CamViewer")

    Q_PROPERTY(QVariantList cameras READ cameras NOTIFY camerasChanged)
    Q_PROPERTY(QVariantMap runtime READ runtime NOTIFY runtimeChanged)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(bool anyRecording READ anyRecording NOTIFY runtimeChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    // Vazio enquanto não houver versão mais nova; senão version, notes, page e url.
    Q_PROPERTY(QVariantMap update READ update NOTIFY updateChanged)

public:
    explicit Manager(QObject *parent = nullptr);
    ~Manager() override;

    QVariantList cameras() const;
    QVariantMap runtime() const;
    QVariantMap settings() const { return m_settings; }
    bool anyRecording() const;
    // Iniciar em segundo plano no login (arquivo em ~/.config/autostart).
    bool autostart() const;
    void setAutostart(bool enabled);
    QString version() const;
    QVariantMap update() const { return m_update; }

    // id vazio cria uma câmera nova; devolve o id.
    Q_INVOKABLE QString saveCamera(const QVariantMap &camera);
    Q_INVOKABLE void removeCamera(const QString &id);
    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);
    // Captura instantânea a partir do quadro que já está na tela.
    Q_INVOKABLE bool snapshotFromSink(const QString &id, QObject *videoSink);
    Q_INVOKABLE void openFolder(const QString &kind);
    // Consulta o latest.json da última release. `manual` responde sempre por
    // updateCheckFinished, mesmo sem novidade.
    Q_INVOKABLE void checkForUpdates(bool manual = false);
    Q_INVOKABLE void openUpdate();

public Q_SLOTS:
    Q_SCRIPTABLE QString State() const;
    Q_SCRIPTABLE void Snapshot(const QString &id);
    Q_SCRIPTABLE void ToggleRecording(const QString &id);
    Q_SCRIPTABLE void ShowWindow(const QString &id);
    // Move uma câmera PTZ (velocidades de -1 a 1) até o PtzStop().
    Q_SCRIPTABLE void PtzMove(const QString &id, double pan, double tilt, double zoom);
    Q_SCRIPTABLE void PtzStop(const QString &id);
    Q_SCRIPTABLE void Quit();

Q_SIGNALS:
    Q_SCRIPTABLE void StateChanged();

    void camerasChanged();
    void runtimeChanged();
    void settingsChanged();
    void autostartChanged();
    void updateChanged();
    void updateCheckFinished(const QString &message);
    void ptzFailed(const QString &id, const QString &message);
    void showWindowRequested(const QString &id);
    // Só fora do KDE: quem tem a bandeja mostra o balão.
    void notificationRequested(const QString &title, const QString &text,
                               const QString &file, const QString &cameraId);

private:
    struct Camera {
        QString id;
        QString name;
        QString url;
        QString subUrl;
        bool motion = false;
        int sensitivity = 5;
        bool autoRecord = false;
        // Só afetam a exibição; gravações e capturas ficam como a câmera envia.
        int rotation = 0;
        bool mirror = false;
        bool ptz = false;
        // 0 = descobrir sozinho.
        int onvifPort = 0;
        // Algumas câmeras andam ao contrário do que o padrão ONVIF define.
        bool ptzInvertPan = false;
        bool ptzInvertTilt = false;
        bool ptzZoom = false;

        QProcess *recorder = nullptr;
        QDateTime recordingSince;
        QString recordingFile;
        QTimer *autoStop = nullptr;
        MotionDetector *detector = nullptr;
        QDateTime lastAlert;
    };

    void load();
    void save() const;
    void applyEnvironment() const;
    void syncDetectors();

    Camera *find(const QString &id);
    QStringList inputArgs(const QString &url) const;
    QString outputPath(const Camera &camera, const QString &kind, const QString &extension) const;

    void startRecording(Camera &camera, int autoStopSeconds = 0);
    void stopRecording(Camera &camera);
    void grabSnapshot(const Camera &camera, const QString &path, std::function<void(bool)> done);
    void onMotion(const QString &id);

    void notify(const QString &event, const QString &title, const QString &text,
                const QString &file = {}, const QString &cameraId = {}, bool offerRecording = false);
    void notifyUpdate();
    void emitRuntimeChanged();

    QList<Camera> m_cameras;
    OnvifPtz *m_ptz;
    QNetworkAccessManager m_network;
    QVariantMap m_update;
    QString m_notifiedVersion;
    QVariantMap m_settings;
};
