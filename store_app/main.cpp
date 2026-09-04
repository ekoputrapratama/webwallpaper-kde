#include <QApplication>
#include <QMessageBox>

#include "configmanager.h"
#include "storewindow.h"
#include "networkmanager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("WebWallpaperStore");
    app.setApplicationDisplayName("Web Wallpaper Store");
    app.setOrganizationName("WebWallpaper");
    app.setWindowIcon(QIcon(":/icons/app.png"));

    ConfigManager config;
    config.loadDotEnv();

    NetworkManager netMgr;
    StoreWindow store(&netMgr, &config);
    store.show();

    return app.exec();
}