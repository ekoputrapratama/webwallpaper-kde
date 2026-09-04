#pragma once

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

namespace KWayland { namespace Client {
class ConnectionThread;
class Registry;
class PlasmaWindowManagement;
class PlasmaWindow;
}}

class WindowMonitor : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool hasFullscreenWindow READ hasFullscreenWindow NOTIFY hasFullscreenWindowChanged)

public:
    explicit WindowMonitor(QObject *parent = nullptr);
    ~WindowMonitor() override;

    bool hasFullscreenWindow() const;

signals:
    void hasFullscreenWindowChanged();

private:
    void rescan();
    void setFullscreen(bool fs);

    // X11
    void setupX11();
    bool m_isX11 = false;
    QSet<quintptr> m_fullscreenWids;

    // Wayland
    void setupWayland();
    void trackWindow(KWayland::Client::PlasmaWindow *window);
    void recheckAll();
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Registry *m_registry = nullptr;
    KWayland::Client::PlasmaWindowManagement *m_management = nullptr;
    QSet<KWayland::Client::PlasmaWindow *> m_windows;
    bool m_hasFullscreen = false;
    bool m_rawFullscreen = false;
    QTimer m_resumeDebounce;

private slots:
    void onRegistryCreated(quint32 name, quint32 version);
    void onWindowFullscreenChanged();
};
