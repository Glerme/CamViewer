#include "manager.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QMenu>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

#ifdef CAMVIEWER_KDE
#include <KStatusNotifierItem>
#include <QDBusConnection>
#include <QDBusInterface>
#else
#include <QDesktopServices>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSystemTrayIcon>
#include <QUrl>
#endif

namespace {

#ifdef CAMVIEWER_KDE

const QString Service = QStringLiteral("org.camviewer.CamViewer");
const QString Path = QStringLiteral("/CamViewer");

// Falso se já existe outra instância; nesse caso ela é avisada para mostrar
// a janela (a não ser que esta tenha sido iniciada em segundo plano).
bool claimSingleInstance(bool background)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.registerService(Service)) {
        return true;
    }
    if (!background) {
        QDBusInterface(Service, Path, Service, bus).call(QStringLiteral("ShowWindow"), QString());
    }
    return false;
}

void listenForRequests(Manager &manager)
{
    QDBusConnection::sessionBus().registerObject(Path, &manager, QDBusConnection::ExportScriptableContents);
}

#else

QString socketName()
{
    return QStringLiteral("camviewer-") + QDir::home().dirName();
}

bool claimSingleInstance(bool background)
{
    QLocalSocket socket;
    socket.connectToServer(socketName());
    if (!socket.waitForConnected(500)) {
        return true;
    }
    if (!background) {
        socket.write("show\n");
        socket.waitForBytesWritten(1000);
    }
    return false;
}

void listenForRequests(Manager &manager)
{
    // Um socket deixado por uma instância que travou impediria o listen().
    QLocalServer::removeServer(socketName());
    auto *server = new QLocalServer(&manager);
    QObject::connect(server, &QLocalServer::newConnection, &manager, [server, &manager] {
        QLocalSocket *client = server->nextPendingConnection();
        QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        QObject::connect(client, &QLocalSocket::readyRead, &manager, [client, &manager] {
            if (client->readAll().startsWith("show")) {
                manager.ShowWindow({});
            }
        });
    });
    server->listen(socketName());
}

#endif

}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("camviewer"));
    app.setApplicationDisplayName(QStringLiteral("CamViewer"));
    app.setApplicationVersion(QStringLiteral(CAMVIEWER_VERSION));
    app.setOrganizationDomain(QStringLiteral("camviewer.org"));
    app.setDesktopFileName(QStringLiteral("org.camviewer.CamViewer"));
    // O processo continua vivo sem janela para gravar e vigiar alertas.
    app.setQuitOnLastWindowClosed(false);

    // Ícones embutidos para quando o sistema não tem tema de ícones.
    QIcon::setFallbackThemeName(QStringLiteral("camviewer"));
#ifdef CAMVIEWER_KDE
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("camera-video")));
#else
    QIcon::setThemeName(QStringLiteral("camviewer"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/camviewer.svg")));
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Visualizador de câmeras IP"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption backgroundOption(QStringLiteral("background"),
                                              QStringLiteral("Inicia sem abrir a janela."));
    parser.addOption(backgroundOption);
    parser.process(app);
    const bool background = parser.isSet(backgroundOption);

    if (!claimSingleInstance(background)) {
        return 0;
    }

    Manager manager;
    listenForRequests(manager);

#ifdef CAMVIEWER_KDE
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }
#endif

    // A interface inteira (motor QML e players) só existe enquanto a janela
    // está aberta; fechar a janela devolve toda essa memória.
    QPointer<QQmlApplicationEngine> engine;
    auto showWindow = [&](const QString &cameraId) {
        if (!engine) {
            engine = new QQmlApplicationEngine(&app);
            engine->rootContext()->setContextProperty(QStringLiteral("manager"), &manager);
            engine->setInitialProperties({{QStringLiteral("focusedCameraId"), cameraId}});
            engine->load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
            if (engine->rootObjects().isEmpty()) {
                delete engine;
                return;
            }
            auto *window = qobject_cast<QQuickWindow *>(engine->rootObjects().constFirst());
            QObject::connect(window, &QQuickWindow::closing, engine, &QObject::deleteLater);
            return;
        }

        auto *window = qobject_cast<QQuickWindow *>(engine->rootObjects().constFirst());
        if (!cameraId.isEmpty()) {
            window->setProperty("focusedCameraId", cameraId);
        }
        window->show();
        window->raise();
        window->requestActivate();
    };
    QObject::connect(&manager, &Manager::showWindowRequested, &app, showWindow);

    auto fillMenu = [&](QMenu *menu) {
        menu->addAction(QIcon::fromTheme(QStringLiteral("camera-video")), QStringLiteral("Abrir CamViewer"), &app, [&] {
            showWindow({});
        });
        menu->addAction(QIcon::fromTheme(QStringLiteral("folder-videos")), QStringLiteral("Gravações"), &manager, [&] {
            manager.openFolder(QStringLiteral("record"));
        });
        menu->addAction(QIcon::fromTheme(QStringLiteral("folder-pictures")), QStringLiteral("Capturas"), &manager, [&] {
            manager.openFolder(QStringLiteral("snapshot"));
        });
        menu->addSeparator();
        menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), QStringLiteral("Sair"), &app, &QApplication::quit);
    };

#ifdef CAMVIEWER_KDE
    KStatusNotifierItem tray;
    tray.setCategory(KStatusNotifierItem::ApplicationStatus);
    tray.setStatus(KStatusNotifierItem::Active);
    tray.setTitle(QStringLiteral("CamViewer"));
    tray.setIconByName(QStringLiteral("camera-video"));
    tray.setStandardActionsEnabled(false);
    fillMenu(tray.contextMenu());
    QObject::connect(&tray, &KStatusNotifierItem::activateRequested, &app, [&] {
        showWindow({});
    });

    auto updateTray = [&] {
        const bool recording = manager.anyRecording();
        tray.setOverlayIconByName(recording ? QStringLiteral("media-record") : QString());
        tray.setToolTip(QStringLiteral("camera-video"), QStringLiteral("CamViewer"),
                        recording ? QStringLiteral("Gravando") : QStringLiteral("Em segundo plano"));
    };
#else
    QMenu menu;
    fillMenu(&menu);
    QSystemTrayIcon tray(app.windowIcon());
    tray.setContextMenu(&menu);
    tray.show();
    QObject::connect(&tray, &QSystemTrayIcon::activated, &app, [&](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showWindow({});
        }
    });

    // O balão do sistema não tem botões: clicar nele abre a câmera do alerta
    // ou o arquivo que acabou de ser salvo.
    QString clickCamera;
    QString clickFile;
    QObject::connect(&manager, &Manager::notificationRequested, &app,
                     [&](const QString &title, const QString &text, const QString &file, const QString &cameraId) {
        clickCamera = cameraId;
        clickFile = file;
        tray.showMessage(title, text, app.windowIcon());
    });
    QObject::connect(&tray, &QSystemTrayIcon::messageClicked, &app, [&] {
        if (!clickCamera.isEmpty()) {
            showWindow(clickCamera);
        } else if (!clickFile.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(clickFile));
        }
    });

    auto updateTray = [&] {
        tray.setToolTip(manager.anyRecording() ? QStringLiteral("CamViewer — gravando") : QStringLiteral("CamViewer"));
    };
#endif
    QObject::connect(&manager, &Manager::runtimeChanged, &app, updateTray);
    updateTray();

    if (!background) {
        showWindow({});
    }

    const int result = app.exec();
    // A interface usa o manager; precisa sumir antes dele.
    delete engine;
    return result;
}
