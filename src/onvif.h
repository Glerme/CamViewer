#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

#include <functional>

// Move câmeras PTZ pelo protocolo ONVIF (SOAP sobre HTTP).
//
// Na primeira vez que uma câmera é usada, descobre o que precisa dela
// (diferença de relógio, endereço dos serviços e o perfil de vídeo) e guarda;
// depois cada movimento é uma única requisição.
class OnvifPtz : public QObject
{
    Q_OBJECT

public:
    struct Target {
        QString host;
        // 0 procura sozinho nas portas mais usadas.
        int port = 0;
        QString user;
        QString password;

        QString key() const { return QStringLiteral("%1:%2@%3").arg(user, QString::number(port), host); }
    };

    explicit OnvifPtz(QObject *parent = nullptr);

    // Velocidades de -1 a 1; a câmera segue se movendo até o stop().
    void move(const QString &cameraId, const Target &target, double pan, double tilt, double zoom);
    void stop(const QString &cameraId, const Target &target);

Q_SIGNALS:
    void failed(const QString &cameraId, const QString &message);

private:
    struct Session {
        int port = 0;
        qint64 clockOffsetSecs = 0;
        QUrl ptzUrl;
        QString profileToken;
        bool ready = false;
    };
    using Reply = std::function<void(bool ok, const QByteArray &body)>;

    void withSession(const QString &cameraId, const Target &target, std::function<void(const Session &)> then);
    void findService(const Target &target, QList<int> ports, std::function<void(int port, const QByteArray &dateTime)> found);
    void openSession(const QString &cameraId, const Target &target, int port, const QByteArray &dateTime,
                     std::function<void(const Session &)> then);
    void post(const QUrl &url, const Target &target, qint64 clockOffsetSecs, bool authenticate,
              const QString &body, Reply reply);
    QUrl serviceUrl(const Target &target, int port, const QString &advertised, const QString &fallbackPath) const;

    QNetworkAccessManager m_network;
    QHash<QString, Session> m_sessions;
    // Muda a cada move()/stop(); um movimento que ainda esperava a descoberta
    // da câmera é descartado se o botão já tiver sido solto.
    QHash<QString, int> m_generation;
};
