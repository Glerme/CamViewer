#include "manager.h"
#include "ffmpeg.h"
#include "motiondetector.h"

#ifdef CAMVIEWER_KDE
#include <KNotification>
#endif

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QVideoFrame>
#include <QVideoSink>

#include <memory>

namespace {

QString configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/camviewer/config.json");
}

#ifndef Q_OS_WIN
QString autostartPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/org.camviewer.CamViewer.desktop");
}
#endif

QVariantMap defaultSettings()
{
    return {
        {QStringLiteral("recordDir"),
         QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + QStringLiteral("/CamViewer")},
        {QStringLiteral("snapshotDir"),
         QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QStringLiteral("/CamViewer")},
        {QStringLiteral("alertCooldown"), 60},
        {QStringLiteral("autoRecordSeconds"), 30},
        {QStringLiteral("rtspTcp"), true},
        {QStringLiteral("lowLatency"), true},
        {QStringLiteral("pauseOnUnfocus"), false},
    };
}

QString safeName(QString name)
{
    static const QRegularExpression unsafe(QStringLiteral("[^\\p{L}\\p{N}_-]+"));
    name.replace(unsafe, QStringLiteral("_"));
    return name.isEmpty() ? QStringLiteral("camera") : name;
}

// Pede para o ffmpeg fechar o arquivo direito; mata se ele não responder.
void finishRecorder(QProcess *recorder, int waitMs)
{
    recorder->write("q\n");
    if (!recorder->waitForFinished(waitMs)) {
        recorder->kill();
        recorder->waitForFinished(1000);
    }
}

}

Manager::Manager(QObject *parent)
    : QObject(parent)
{
    load();
    applyEnvironment();
    syncDetectors();
}

Manager::~Manager()
{
    for (Camera &camera : m_cameras) {
        delete camera.detector;
    }
    // Inclui gravações que já receberam o pedido de parada e ainda estão
    // fechando o arquivo.
    const auto processes = findChildren<QProcess *>(Qt::FindDirectChildrenOnly);
    for (QProcess *process : processes) {
        process->disconnect(this);
        if (process->state() != QProcess::NotRunning) {
            finishRecorder(process, 3000);
        }
    }
}

QVariantList Manager::cameras() const
{
    QVariantList list;
    for (const Camera &camera : m_cameras) {
        list << QVariantMap{
            {QStringLiteral("id"), camera.id},
            {QStringLiteral("name"), camera.name},
            {QStringLiteral("url"), camera.url},
            {QStringLiteral("subUrl"), camera.subUrl},
            {QStringLiteral("motion"), camera.motion},
            {QStringLiteral("sensitivity"), camera.sensitivity},
            {QStringLiteral("autoRecord"), camera.autoRecord},
        };
    }
    return list;
}

QVariantMap Manager::runtime() const
{
    QVariantMap map;
    for (const Camera &camera : m_cameras) {
        map.insert(camera.id, QVariantMap{
            {QStringLiteral("recording"), camera.recorder != nullptr},
            {QStringLiteral("since"), camera.recorder ? camera.recordingSince.toMSecsSinceEpoch() : 0},
        });
    }
    return map;
}

bool Manager::anyRecording() const
{
    return std::any_of(m_cameras.cbegin(), m_cameras.cend(), [](const Camera &camera) {
        return camera.recorder != nullptr;
    });
}

#ifdef Q_OS_WIN

namespace {
const QString RunKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString RunValue = QStringLiteral("CamViewer");
}

bool Manager::autostart() const
{
    return QSettings(RunKey, QSettings::NativeFormat).contains(RunValue);
}

void Manager::setAutostart(bool enabled)
{
    if (enabled == autostart()) {
        return;
    }

    QSettings run(RunKey, QSettings::NativeFormat);
    if (enabled) {
        const QString program = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        run.setValue(RunValue, QStringLiteral("\"%1\" --background").arg(program));
    } else {
        run.remove(RunValue);
    }
    Q_EMIT autostartChanged();
}

#else

bool Manager::autostart() const
{
    return QFile::exists(autostartPath());
}

void Manager::setAutostart(bool enabled)
{
    if (enabled == autostart()) {
        return;
    }

    const QString path = autostartPath();
    if (!enabled) {
        QFile::remove(path);
    } else {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qWarning("Não foi possível criar %s", qPrintable(path));
            return;
        }
        const QString entry = QStringLiteral(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=CamViewer\n"
            "Comment=Inicia o CamViewer em segundo plano\n"
            "Exec=\"%1\" --background\n"
            "Icon=camera-video\n"
            "Terminal=false\n"
            "X-KDE-autostart-phase=2\n").arg(QCoreApplication::applicationFilePath());
        file.write(entry.toUtf8());
    }
    Q_EMIT autostartChanged();
}

#endif

QString Manager::saveCamera(const QVariantMap &data)
{
    QString id = data.value(QStringLiteral("id")).toString();
    Camera *camera = find(id);
    if (!camera) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        m_cameras.append(Camera{.id = id});
        camera = &m_cameras.last();
    }

    camera->name = data.value(QStringLiteral("name")).toString().trimmed();
    camera->url = data.value(QStringLiteral("url")).toString().trimmed();
    camera->subUrl = data.value(QStringLiteral("subUrl")).toString().trimmed();
    camera->motion = data.value(QStringLiteral("motion")).toBool();
    camera->sensitivity = data.value(QStringLiteral("sensitivity"), 5).toInt();
    camera->autoRecord = data.value(QStringLiteral("autoRecord")).toBool();
    if (camera->name.isEmpty()) {
        camera->name = tr("Câmera %1").arg(m_cameras.size());
    }

    save();
    syncDetectors();
    Q_EMIT camerasChanged();
    emitRuntimeChanged();
    return id;
}

void Manager::removeCamera(const QString &id)
{
    for (qsizetype i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i].id != id) {
            continue;
        }
        stopRecording(m_cameras[i]);
        delete m_cameras[i].detector;
        m_cameras.removeAt(i);
        break;
    }
    save();
    Q_EMIT camerasChanged();
    emitRuntimeChanged();
}

void Manager::setSetting(const QString &key, const QVariant &value)
{
    if (m_settings.value(key) == value) {
        return;
    }
    m_settings.insert(key, value);
    save();
    applyEnvironment();
    if (key == QLatin1String("rtspTcp")) {
        // Os detectores já rodando ainda usam o transporte antigo.
        for (Camera &camera : m_cameras) {
            if (camera.detector) {
                camera.detector->stop();
            }
        }
        syncDetectors();
    }
    Q_EMIT settingsChanged();
    Q_EMIT StateChanged();
}

bool Manager::snapshotFromSink(const QString &id, QObject *videoSink)
{
    Camera *camera = find(id);
    if (!camera) {
        return false;
    }

    QImage image;
    if (auto *sink = qobject_cast<QVideoSink *>(videoSink)) {
        image = sink->videoFrame().toImage();
    }
    const QString path = outputPath(*camera, QStringLiteral("snapshot"), QStringLiteral("jpg"));
    if (image.isNull() || !image.save(path, "JPG", 92)) {
        Snapshot(id);
        return false;
    }
    notify(QStringLiteral("snapshot"), tr("Captura salva"), camera->name, path);
    return true;
}

void Manager::openFolder(const QString &kind)
{
    const QString dir = m_settings.value(kind == QLatin1String("record") ? QStringLiteral("recordDir")
                                                                         : QStringLiteral("snapshotDir")).toString();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

QString Manager::State() const
{
    QJsonArray cameras;
    for (const Camera &camera : m_cameras) {
        cameras.append(QJsonObject{
            {QStringLiteral("id"), camera.id},
            {QStringLiteral("name"), camera.name},
            {QStringLiteral("url"), camera.url},
            {QStringLiteral("subUrl"), camera.subUrl},
            {QStringLiteral("motion"), camera.motion},
            {QStringLiteral("recording"), camera.recorder != nullptr},
            {QStringLiteral("since"), camera.recorder ? camera.recordingSince.toMSecsSinceEpoch() : 0},
        });
    }
    const QJsonObject state{
        {QStringLiteral("cameras"), cameras},
        {QStringLiteral("lowLatency"), m_settings.value(QStringLiteral("lowLatency")).toBool()},
    };
    return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
}

void Manager::Snapshot(const QString &id)
{
    Camera *camera = find(id);
    if (!camera) {
        return;
    }
    const QString name = camera->name;
    const QString path = outputPath(*camera, QStringLiteral("snapshot"), QStringLiteral("jpg"));
    grabSnapshot(*camera, path, [this, name, path](bool ok) {
        if (ok) {
            notify(QStringLiteral("snapshot"), tr("Captura salva"), name, path);
        } else {
            notify(QStringLiteral("snapshot"), tr("Falha na captura"),
                   tr("Não foi possível obter uma imagem de %1.").arg(name));
        }
    });
}

void Manager::ToggleRecording(const QString &id)
{
    Camera *camera = find(id);
    if (!camera) {
        return;
    }
    if (camera->recorder) {
        stopRecording(*camera);
    } else {
        startRecording(*camera);
    }
}

void Manager::ShowWindow(const QString &id)
{
    Q_EMIT showWindowRequested(id);
}

void Manager::Quit()
{
    QCoreApplication::quit();
}

void Manager::load()
{
    m_settings = defaultSettings();

    QFile file(configPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();

    const QVariantMap stored = root.value(QLatin1String("settings")).toObject().toVariantMap();
    for (auto it = stored.cbegin(); it != stored.cend(); ++it) {
        m_settings.insert(it.key(), it.value());
    }

    const QJsonArray cameras = root.value(QLatin1String("cameras")).toArray();
    for (const QJsonValue &value : cameras) {
        const QJsonObject object = value.toObject();
        Camera camera;
        camera.id = object.value(QLatin1String("id")).toString();
        camera.name = object.value(QLatin1String("name")).toString();
        camera.url = object.value(QLatin1String("url")).toString();
        camera.subUrl = object.value(QLatin1String("subUrl")).toString();
        camera.motion = object.value(QLatin1String("motion")).toBool();
        camera.sensitivity = object.value(QLatin1String("sensitivity")).toInt(5);
        camera.autoRecord = object.value(QLatin1String("autoRecord")).toBool();
        if (!camera.id.isEmpty()) {
            m_cameras.append(camera);
        }
    }
}

void Manager::save() const
{
    const QString path = configPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("Não foi possível salvar %s", qPrintable(path));
        return;
    }
    // As URLs costumam conter usuário e senha da câmera.
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);

    const QJsonObject root{
        {QStringLiteral("settings"), QJsonObject::fromVariantMap(m_settings)},
        {QStringLiteral("cameras"), QJsonArray::fromVariantList(cameras())},
    };
    file.write(QJsonDocument(root).toJson());
}

void Manager::applyEnvironment() const
{
    // O backend FFmpeg do Qt Multimedia lê esta variável ao abrir um stream.
    if (m_settings.value(QStringLiteral("rtspTcp")).toBool()) {
        qputenv("QT_FFMPEG_RTSP_TRANSPORT", "tcp");
    } else {
        qunsetenv("QT_FFMPEG_RTSP_TRANSPORT");
    }
}

void Manager::syncDetectors()
{
    for (Camera &camera : m_cameras) {
        if (!camera.motion || camera.url.isEmpty()) {
            delete camera.detector;
            camera.detector = nullptr;
            continue;
        }
        if (!camera.detector) {
            camera.detector = new MotionDetector(this);
            const QString id = camera.id;
            connect(camera.detector, &MotionDetector::motion, this, [this, id] {
                onMotion(id);
            });
        }
        camera.detector->setSensitivity(camera.sensitivity);
        // O substream, quando existe, gasta bem menos CPU para analisar.
        camera.detector->start(inputArgs(camera.subUrl.isEmpty() ? camera.url : camera.subUrl));
    }
}

Manager::Camera *Manager::find(const QString &id)
{
    for (Camera &camera : m_cameras) {
        if (camera.id == id) {
            return &camera;
        }
    }
    return nullptr;
}

QStringList Manager::inputArgs(const QString &url) const
{
    QStringList args{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error")};
    if (url.startsWith(QLatin1String("rtsp"), Qt::CaseInsensitive)) {
        if (m_settings.value(QStringLiteral("rtspTcp")).toBool()) {
            args << QStringLiteral("-rtsp_transport") << QStringLiteral("tcp");
        }
        // Sem isto o ffmpeg fica preso para sempre se a câmera sumir da rede;
        // com o limite ele sai, fecha o arquivo e o detector reconecta.
        args << QStringLiteral("-timeout") << QStringLiteral("15000000");
    }
    args << QStringLiteral("-i") << url;
    return args;
}

QString Manager::outputPath(const Camera &camera, const QString &kind, const QString &extension) const
{
    const bool record = kind == QLatin1String("record");
    const QString base = m_settings.value(record ? QStringLiteral("recordDir") : QStringLiteral("snapshotDir")).toString();
    const QString name = safeName(camera.name);
    const QString dir = base + QLatin1Char('/') + name;
    QDir().mkpath(dir);

    const QString prefix = kind == QLatin1String("alert") ? QStringLiteral("alerta_") : QString();
    return QStringLiteral("%1/%2%3_%4.%5")
        .arg(dir, prefix, name, QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss-zzz")), extension);
}

void Manager::startRecording(Camera &camera, int autoStopSeconds)
{
    if (camera.recorder || camera.url.isEmpty()) {
        return;
    }

    const QString id = camera.id;
    const QString path = outputPath(camera, QStringLiteral("record"), QStringLiteral("mkv"));

    // Copia o stream sem recodificar: quase nada de CPU e qualidade original.
    // Matroska aceita os codecs de áudio comuns em câmeras e sobrevive a quedas.
    QStringList args = inputArgs(camera.url);
    args << QStringLiteral("-map") << QStringLiteral("0:v")
         << QStringLiteral("-map") << QStringLiteral("0:a?")
         << QStringLiteral("-c") << QStringLiteral("copy")
         << QStringLiteral("-f") << QStringLiteral("matroska")
         << QStringLiteral("-y") << path;

    auto *recorder = new QProcess(this);
    recorder->setStandardOutputFile(QProcess::nullDevice());
    recorder->setStandardErrorFile(QProcess::nullDevice());

    // Só chega aqui se o ffmpeg morrer sozinho; stopRecording() desconecta antes.
    auto failed = [this, id, recorder] {
        Camera *camera = find(id);
        if (!camera || camera->recorder != recorder) {
            return;
        }
        const QString file = camera->recordingFile;
        const bool hasData = QFileInfo(file).size() > 0;
        camera->recorder = nullptr;
        delete camera->autoStop;
        camera->autoStop = nullptr;
        recorder->deleteLater();
        notify(QStringLiteral("recording"), tr("Gravação interrompida"),
               tr("A gravação de %1 parou inesperadamente.").arg(camera->name),
               hasData ? file : QString(), id);
        emitRuntimeChanged();
    };
    connect(recorder, &QProcess::finished, this, failed);
    connect(recorder, &QProcess::errorOccurred, this, [failed](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            failed();
        }
    });

    camera.recorder = recorder;
    camera.recordingSince = QDateTime::currentDateTime();
    camera.recordingFile = path;
    recorder->start(ffmpegProgram(), args);

    if (autoStopSeconds > 0) {
        camera.autoStop = new QTimer(this);
        camera.autoStop->setSingleShot(true);
        connect(camera.autoStop, &QTimer::timeout, this, [this, id] {
            if (Camera *camera = find(id)) {
                stopRecording(*camera);
            }
        });
        camera.autoStop->start(autoStopSeconds * 1000);
    }

    emitRuntimeChanged();
}

void Manager::stopRecording(Camera &camera)
{
    if (!camera.recorder) {
        return;
    }

    QProcess *recorder = camera.recorder;
    const QString file = camera.recordingFile;
    const QString name = camera.name;
    camera.recorder = nullptr;
    delete camera.autoStop;
    camera.autoStop = nullptr;

    recorder->disconnect(this);
    auto finished = [this, recorder, file, name] {
        recorder->deleteLater();
        if (QFileInfo(file).size() > 0) {
            notify(QStringLiteral("recording"), tr("Gravação salva"), name, file);
        } else {
            QFile::remove(file);
            notify(QStringLiteral("recording"), tr("Gravação vazia"),
                   tr("Nenhum vídeo foi recebido de %1.").arg(name));
        }
    };

    if (recorder->state() == QProcess::NotRunning) {
        finished();
    } else {
        connect(recorder, &QProcess::finished, this, finished);
        recorder->write("q\n");
        QTimer::singleShot(4000, recorder, &QProcess::kill);
    }

    emitRuntimeChanged();
}

void Manager::grabSnapshot(const Camera &camera, const QString &path, std::function<void(bool)> done)
{
    QStringList args = inputArgs(camera.url);
    args << QStringLiteral("-frames:v") << QStringLiteral("1")
         << QStringLiteral("-q:v") << QStringLiteral("2")
         << QStringLiteral("-y") << path;

    auto *process = new QProcess(this);
    process->setStandardOutputFile(QProcess::nullDevice());
    process->setStandardErrorFile(QProcess::nullDevice());

    // finished e errorOccurred podem chegar os dois; responde uma vez só.
    auto called = std::make_shared<bool>(false);
    auto finish = [process, path, called, done = std::move(done)] {
        if (*called) {
            return;
        }
        *called = true;
        process->deleteLater();
        done(process->exitStatus() == QProcess::NormalExit && process->exitCode() == 0 && QFileInfo(path).size() > 0);
    };
    connect(process, &QProcess::finished, this, finish);
    connect(process, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finish();
        }
    });
    QTimer::singleShot(15000, process, &QProcess::kill);
    process->start(ffmpegProgram(), args);
}

void Manager::onMotion(const QString &id)
{
    Camera *camera = find(id);
    if (!camera) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    const int cooldown = m_settings.value(QStringLiteral("alertCooldown")).toInt();
    if (camera->lastAlert.isValid() && camera->lastAlert.secsTo(now) < cooldown) {
        // Movimento contínuo: mantém a gravação automática rodando.
        if (camera->autoStop) {
            camera->autoStop->start();
        }
        return;
    }
    camera->lastAlert = now;

    if (camera->autoRecord && !camera->recorder) {
        startRecording(*camera, m_settings.value(QStringLiteral("autoRecordSeconds")).toInt());
    }

    const QString name = camera->name;
    const QString time = QLocale().toString(now.time(), QLocale::ShortFormat);
    const QString path = outputPath(*camera, QStringLiteral("alert"), QStringLiteral("jpg"));
    grabSnapshot(*camera, path, [this, id, name, time, path](bool ok) {
        const Camera *camera = find(id);
        notify(QStringLiteral("motion"), tr("Movimento em %1").arg(name),
               tr("Detectado às %1").arg(time), ok ? path : QString(), id,
               camera && !camera->recorder);
    });
}

void Manager::notify(const QString &event, const QString &title, const QString &text,
                     const QString &file, const QString &cameraId, bool offerRecording)
{
#ifdef CAMVIEWER_KDE
    auto *notification = new KNotification(event, KNotification::CloseOnTimeout);
    notification->setComponentName(QStringLiteral("camviewer"));
    notification->setTitle(title);
    notification->setText(text);
    notification->setIconName(QStringLiteral("camera-video"));
    if (!file.isEmpty()) {
        notification->setUrls({QUrl::fromLocalFile(file)});
    }

    if (!cameraId.isEmpty()) {
        auto *view = notification->addDefaultAction(tr("Ver câmera"));
        connect(view, &KNotificationAction::activated, this, [this, cameraId] {
            ShowWindow(cameraId);
        });
        if (offerRecording) {
            auto *record = notification->addAction(tr("Gravar"));
            connect(record, &KNotificationAction::activated, this, [this, cameraId] {
                Camera *camera = find(cameraId);
                if (camera && !camera->recorder) {
                    startRecording(*camera);
                }
            });
        }
    } else if (!file.isEmpty()) {
        auto *open = notification->addDefaultAction(tr("Abrir"));
        connect(open, &KNotificationAction::activated, this, [file] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(file));
        });
    }

    notification->sendEvent();
#else
    Q_UNUSED(event)
    Q_UNUSED(offerRecording)
    Q_EMIT notificationRequested(title, text, file, cameraId);
#endif
}

void Manager::emitRuntimeChanged()
{
    Q_EMIT runtimeChanged();
    Q_EMIT StateChanged();
}
