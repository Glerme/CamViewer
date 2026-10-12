#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class MotionDetector;

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

    // id vazio cria uma câmera nova; devolve o id.
    Q_INVOKABLE QString saveCamera(const QVariantMap &camera);
    Q_INVOKABLE void removeCamera(const QString &id);
    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);
    // Captura instantânea a partir do quadro que já está na tela.
    Q_INVOKABLE bool snapshotFromSink(const QString &id, QObject *videoSink);
    Q_INVOKABLE void openFolder(const QString &kind);

public Q_SLOTS:
    Q_SCRIPTABLE QString State() const;
    Q_SCRIPTABLE void Snapshot(const QString &id);
    Q_SCRIPTABLE void ToggleRecording(const QString &id);
    Q_SCRIPTABLE void ShowWindow(const QString &id);
    Q_SCRIPTABLE void Quit();

Q_SIGNALS:
    Q_SCRIPTABLE void StateChanged();

    void camerasChanged();
    void runtimeChanged();
    void settingsChanged();
    void autostartChanged();
    void showWindowRequested(const QString &id);

private:
    struct Camera {
        QString id;
        QString name;
        QString url;
        QString subUrl;
        bool motion = false;
        int sensitivity = 5;
        bool autoRecord = false;

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
    void emitRuntimeChanged();

    QList<Camera> m_cameras;
    QVariantMap m_settings;
};
