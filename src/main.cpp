#include "manager.h"

#include <KStatusNotifierItem>

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QIcon>
#include <QMenu>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

namespace {
const QString Service = QStringLiteral("org.camviewer.CamViewer");
const QString Path = QStringLiteral("/CamViewer");
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("camviewer"));
    app.setApplicationDisplayName(QStringLiteral("CamViewer"));
    app.setApplicationVersion(QStringLiteral(CAMVIEWER_VERSION));
    app.setOrganizationDomain(QStringLiteral("camviewer.org"));
    app.setDesktopFileName(QStringLiteral("org.camviewer.CamViewer"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("camera-video")));
    // O processo continua vivo sem janela para gravar e vigiar alertas.
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Visualizador de câmeras IP para o KDE"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption backgroundOption(QStringLiteral("background"),
                                              QStringLiteral("Inicia sem abrir a janela."));
    parser.addOption(backgroundOption);
    parser.process(app);
    const bool background = parser.isSet(backgroundOption);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(Service)) {
        // Já existe uma instância: só pede para ela mostrar a janela.
        if (!background) {
            QDBusInterface(Service, Path, Service, bus).call(QStringLiteral("ShowWindow"), QString());
        }
        return 0;
    }

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    Manager manager;
    bus.registerObject(Path, &manager, QDBusConnection::ExportScriptableContents);

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

    KStatusNotifierItem tray;
    tray.setCategory(KStatusNotifierItem::ApplicationStatus);
    tray.setStatus(KStatusNotifierItem::Active);
    tray.setTitle(QStringLiteral("CamViewer"));
    tray.setIconByName(QStringLiteral("camera-video"));
    tray.setStandardActionsEnabled(false);

    QMenu *menu = tray.contextMenu();
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

    QObject::connect(&tray, &KStatusNotifierItem::activateRequested, &app, [&] {
        showWindow({});
    });

    auto updateTray = [&] {
        const bool recording = manager.anyRecording();
        tray.setOverlayIconByName(recording ? QStringLiteral("media-record") : QString());
        tray.setToolTip(QStringLiteral("camera-video"), QStringLiteral("CamViewer"),
                        recording ? QStringLiteral("Gravando") : QStringLiteral("Em segundo plano"));
    };
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
