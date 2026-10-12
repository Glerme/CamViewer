#include "onvif.h"

#include <QAuthenticator>
#include <QCryptographicHash>
#include <QDateTime>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTimeZone>
#include <QXmlStreamReader>

namespace {

const QString DeviceNs = QStringLiteral("http://www.onvif.org/ver10/device/wsdl");
const QString MediaNs = QStringLiteral("http://www.onvif.org/ver10/media/wsdl");
const QString PtzNs = QStringLiteral("http://www.onvif.org/ver20/ptz/wsdl");
const QString SchemaNs = QStringLiteral("http://www.onvif.org/ver10/schema");

// Texto do primeiro elemento `name` que estiver dentro de um elemento
// `parent` (ou em qualquer lugar, se parent for vazio). Os prefixos de
// namespace variam de fabricante para fabricante, então só o nome local conta.
QString textOf(const QByteArray &xml, const QString &parent, const QString &name)
{
    QXmlStreamReader reader(xml);
    int insideParent = parent.isEmpty() ? 1 : 0;
    while (!reader.atEnd()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (insideParent > 0 && reader.name() == name) {
                return reader.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
            }
            if (!parent.isEmpty() && reader.name() == parent) {
                ++insideParent;
            }
        } else if (token == QXmlStreamReader::EndElement && !parent.isEmpty() && reader.name() == parent) {
            --insideParent;
        }
    }
    return {};
}

QString firstProfileToken(const QByteArray &xml)
{
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        if (reader.readNext() == QXmlStreamReader::StartElement && reader.name() == QLatin1String("Profiles")) {
            return reader.attributes().value(QLatin1String("token")).toString();
        }
    }
    return {};
}

// Hora da câmera em relação à nossa. O cabeçalho de autenticação leva um
// horário, e muitas câmeras recusam o pedido se ele não bater com o delas.
qint64 clockOffset(const QByteArray &xml)
{
    const auto number = [&xml](const QString &parent, const QString &name) {
        return textOf(xml, parent, name).toInt();
    };
    const QDate date(number(QStringLiteral("Date"), QStringLiteral("Year")),
                     number(QStringLiteral("Date"), QStringLiteral("Month")),
                     number(QStringLiteral("Date"), QStringLiteral("Day")));
    const QTime time(number(QStringLiteral("Time"), QStringLiteral("Hour")),
                     number(QStringLiteral("Time"), QStringLiteral("Minute")),
                     number(QStringLiteral("Time"), QStringLiteral("Second")));
    if (!date.isValid() || !time.isValid()) {
        return 0;
    }
    return QDateTime::currentDateTimeUtc().secsTo(QDateTime(date, time, QTimeZone::UTC));
}

QString securityHeader(const OnvifPtz::Target &target, qint64 clockOffsetSecs)
{
    QByteArray nonce(16, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(nonce.data()), nonce.size() / 4);

    const QByteArray created = QDateTime::currentDateTimeUtc().addSecs(clockOffsetSecs)
                                   .toString(QStringLiteral("yyyy-MM-ddTHH:mm:ssZ")).toUtf8();
    const QByteArray digest = QCryptographicHash::hash(nonce + created + target.password.toUtf8(),
                                                       QCryptographicHash::Sha1).toBase64();

    return QStringLiteral(
        "<s:Header>"
        "<Security s:mustUnderstand=\"1\" xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\">"
        "<UsernameToken>"
        "<Username>%1</Username>"
        "<Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">%2</Password>"
        "<Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">%3</Nonce>"
        "<Created xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">%4</Created>"
        "</UsernameToken>"
        "</Security>"
        "</s:Header>")
        .arg(target.user.toHtmlEscaped(), QString::fromLatin1(digest),
             QString::fromLatin1(nonce.toBase64()), QString::fromLatin1(created));
}

}

OnvifPtz::OnvifPtz(QObject *parent)
    : QObject(parent)
{
    // Algumas câmeras pedem autenticação HTTP além (ou em vez) da do SOAP.
    connect(&m_network, &QNetworkAccessManager::authenticationRequired, this,
            [](QNetworkReply *reply, QAuthenticator *authenticator) {
        authenticator->setUser(reply->property("onvifUser").toString());
        authenticator->setPassword(reply->property("onvifPassword").toString());
    });
}

void OnvifPtz::move(const QString &cameraId, const Target &target, double pan, double tilt, double zoom)
{
    const int generation = ++m_generation[cameraId];
    withSession(cameraId, target, [=, this](const Session &session) {
        if (m_generation.value(cameraId) != generation) {
            return;
        }
        // Câmeras sem zoom motorizado recusam o pedido se ele vier com Zoom.
        QString velocity;
        if (pan != 0 || tilt != 0) {
            velocity += QStringLiteral("<PanTilt x=\"%1\" y=\"%2\" xmlns=\"%3\"/>").arg(pan).arg(tilt).arg(SchemaNs);
        }
        if (zoom != 0) {
            velocity += QStringLiteral("<Zoom x=\"%1\" xmlns=\"%2\"/>").arg(zoom).arg(SchemaNs);
        }
        const QString body = QStringLiteral(
            "<ContinuousMove xmlns=\"%1\"><ProfileToken>%2</ProfileToken><Velocity>%3</Velocity></ContinuousMove>")
            .arg(PtzNs, session.profileToken.toHtmlEscaped(), velocity);

        post(session.ptzUrl, target, session.clockOffsetSecs, true, body, [=, this](bool ok, const QByteArray &reply) {
            if (!ok) {
                Q_EMIT failed(cameraId, QString::fromUtf8(reply));
            }
        });
    });
}

void OnvifPtz::stop(const QString &cameraId, const Target &target)
{
    ++m_generation[cameraId];
    // Sem sessão não houve movimento nenhum para parar.
    const auto it = m_sessions.constFind(target.key());
    if (it == m_sessions.constEnd() || !it->ready) {
        return;
    }
    const QString body = QStringLiteral(
        "<Stop xmlns=\"%1\"><ProfileToken>%2</ProfileToken><PanTilt>true</PanTilt><Zoom>true</Zoom></Stop>")
        .arg(PtzNs, it->profileToken.toHtmlEscaped());
    post(it->ptzUrl, target, it->clockOffsetSecs, true, body, [=, this](bool ok, const QByteArray &reply) {
        if (!ok) {
            Q_EMIT failed(cameraId, QString::fromUtf8(reply));
        }
    });
}

void OnvifPtz::withSession(const QString &cameraId, const Target &target, std::function<void(const Session &)> then)
{
    const QString key = target.key();
    if (m_sessions.value(key).ready) {
        then(m_sessions.value(key));
        return;
    }

    // A porta do ONVIF varia com o fabricante e raramente é a mesma da página
    // web da câmera; a informada (se houver) é só a primeira tentativa.
    QList<int> ports{80, 8899, 8080, 2020, 8000, 5000};
    if (target.port > 0) {
        ports.removeAll(target.port);
        ports.prepend(target.port);
    }
    findService(target, ports, [=, this](int port, const QByteArray &dateTime) {
        if (port == 0) {
            Q_EMIT failed(cameraId, tr("Nenhum serviço ONVIF respondeu em %1. Verifique se o ONVIF está ativado na câmera.")
                                        .arg(target.host));
            return;
        }
        openSession(cameraId, target, port, dateTime, then);
    });
}

void OnvifPtz::findService(const Target &target, QList<int> ports,
                           std::function<void(int port, const QByteArray &dateTime)> found)
{
    if (ports.isEmpty()) {
        found(0, {});
        return;
    }
    const int port = ports.takeFirst();
    // Pedir a hora não exige senha, e a resposta já serve para acertar o
    // relógio da autenticação.
    post(serviceUrl(target, port, {}, QStringLiteral("/onvif/device_service")), target, 0, false,
         QStringLiteral("<GetSystemDateAndTime xmlns=\"%1\"/>").arg(DeviceNs),
         [=, this](bool ok, const QByteArray &reply) {
        // Um servidor web comum responde 200 com uma página qualquer.
        if (ok && reply.contains("GetSystemDateAndTimeResponse")) {
            found(port, reply);
        } else {
            findService(target, ports, found);
        }
    });
}

void OnvifPtz::openSession(const QString &cameraId, const Target &target, int port, const QByteArray &dateTime,
                           std::function<void(const Session &)> then)
{
    const QString key = target.key();
    const qint64 offset = clockOffset(dateTime);
    const QUrl deviceUrl = serviceUrl(target, port, {}, QStringLiteral("/onvif/device_service"));
    auto fail = [this, cameraId, key](const QByteArray &message) {
        m_sessions.remove(key);
        Q_EMIT failed(cameraId, QString::fromUtf8(message));
    };

    // Onde ficam os serviços de mídia e de PTZ.
    post(deviceUrl, target, offset, true,
         QStringLiteral("<GetCapabilities xmlns=\"%1\"><Category>All</Category></GetCapabilities>").arg(DeviceNs),
         [=, this](bool ok, const QByteArray &reply) {
        if (!ok) {
            fail(reply);
            return;
        }
        const QString ptz = textOf(reply, QStringLiteral("PTZ"), QStringLiteral("XAddr"));
        if (ptz.isEmpty()) {
            fail(tr("Esta câmera não informa suporte a movimento (PTZ).").toUtf8());
            return;
        }
        const QUrl ptzUrl = serviceUrl(target, port, ptz, QStringLiteral("/onvif/ptz_service"));
        const QUrl mediaUrl = serviceUrl(target, port, textOf(reply, QStringLiteral("Media"), QStringLiteral("XAddr")),
                                         QStringLiteral("/onvif/media_service"));

        // O perfil de vídeo, que identifica o que mover.
        post(mediaUrl, target, offset, true, QStringLiteral("<GetProfiles xmlns=\"%1\"/>").arg(MediaNs),
             [=, this](bool ok, const QByteArray &reply) {
            if (!ok) {
                fail(reply);
                return;
            }
            const QString token = firstProfileToken(reply);
            if (token.isEmpty()) {
                fail(tr("A câmera não devolveu nenhum perfil de vídeo.").toUtf8());
                return;
            }
            Session &session = m_sessions[key];
            session.port = port;
            session.clockOffsetSecs = offset;
            session.ptzUrl = ptzUrl;
            session.profileToken = token;
            session.ready = true;
            then(session);
        });
    });
}

void OnvifPtz::post(const QUrl &url, const Target &target, qint64 clockOffsetSecs, bool authenticate,
                    const QString &body, Reply reply)
{
    const QString envelope = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">%1<s:Body>%2</s:Body></s:Envelope>")
        .arg(authenticate ? securityHeader(target, clockOffsetSecs) : QString(), body);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/soap+xml; charset=utf-8"));
    request.setTransferTimeout(4000);

    QNetworkReply *network = m_network.post(request, envelope.toUtf8());
    network->setProperty("onvifUser", target.user);
    network->setProperty("onvifPassword", target.password);
    connect(network, &QNetworkReply::finished, this, [network, reply = std::move(reply)] {
        network->deleteLater();
        const QByteArray data = network->readAll();
        if (network->error() == QNetworkReply::NoError) {
            reply(true, data);
            return;
        }
        // Um erro SOAP traz a explicação da câmera; sem ele, vale o da rede.
        const QString reason = textOf(data, QStringLiteral("Reason"), QStringLiteral("Text"));
        reply(false, (reason.isEmpty() ? network->errorString() : reason).toUtf8());
    });
}

QUrl OnvifPtz::serviceUrl(const Target &target, int port, const QString &advertised, const QString &fallbackPath) const
{
    // A câmera anuncia o endereço com o IP que ela acha que tem, que pode não
    // ser o que alcançamos; aproveita só o caminho.
    const QUrl announced(advertised);
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(target.host);
    url.setPort(port);
    url.setPath(announced.path().isEmpty() ? fallbackPath : announced.path());
    return url;
}
