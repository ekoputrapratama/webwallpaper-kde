#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>

#include "configmanager.h"
#include "storewindow.h"
#include "networkmanager.h"

namespace {

constexpr auto kPipeName = "webwallpaper-store";

void forwardArgumentsTo(QLocalSocket *socket)
{
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 2)
        return;
    for (int i = 1; i < args.size(); ++i) {
        socket->write(args.at(i).toUtf8().append('\n'));
    }
    socket->flush();
    socket->waitForBytesWritten(100);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("WebWallpaperStore");
    app.setApplicationDisplayName("Web Wallpaper Store");
    app.setOrganizationName("WebWallpaper");
    app.setWindowIcon(QIcon(":/icons/app.png"));

    ConfigManager config;
    config.loadDotEnv();

    // Single-instance pipe, also the deep-link entry point: when the store is
    // opened via webwallpaper://... while already running, forward any URL to
    // the existing window and exit instead of spawning a duplicate.
    QLocalServer server;
    {
        QLocalSocket probe;
        probe.connectToServer(QString::fromLatin1(kPipeName));
        if (probe.waitForConnected(100)) {
            forwardArgumentsTo(&probe);
            probe.disconnectFromServer();
            return 0;
        }
        // Socket left behind by a crashed instance: clear it and take over.
        QLocalServer::removeServer(QString::fromLatin1(kPipeName));
        server.listen(QString::fromLatin1(kPipeName));
    }

    NetworkManager netMgr;
    StoreWindow store(&netMgr, &config);

    // A later deep-link launch reaches the running instance through this pipe;
    // bring the store back to the foreground.
    QObject::connect(&server, &QLocalServer::newConnection, &store, [&server, &store] {
        QLocalSocket *conn = server.nextPendingConnection();
        store.raise();
        store.activateWindow();
        conn->deleteLater();
    });

    store.show();

    return app.exec();
}