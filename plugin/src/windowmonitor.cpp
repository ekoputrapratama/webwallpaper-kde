#include "windowmonitor.h"

#include <KWindowSystem>
#include <QDir>
#include <QFile>
#include <QTextStream>

#ifdef Q_OS_LINUX
// X11 includes
#include <KX11Extras>
#include <KWindowInfo>
#include <netwm.h>
// Wayland includes
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/plasmawindowmanagement.h>
#include <wayland-client.h>
#endif

static void wmLog(const QString &msg)
{
    QFile f(QDir::tempPath() + QStringLiteral("/webwallpaper_wm.log"));
    if (f.open(QIODevice::Append | QIODevice::WriteOnly)) {
        QTextStream out(&f);
        out << msg << Qt::endl;
    }
}

WindowMonitor::WindowMonitor(QObject *parent)
    : QObject(parent)
{
    m_resumeDebounce.setSingleShot(true);
    m_resumeDebounce.setInterval(1500);
    connect(&m_resumeDebounce, &QTimer::timeout, this, [this]() {
        if (!m_rawFullscreen && m_hasFullscreen) {
            m_hasFullscreen = false;
            wmLog(QStringLiteral("WindowMonitor: hasFullscreen changed to 0 (debounced)"));
            emit hasFullscreenWindowChanged();
        }
    });

#ifdef Q_OS_LINUX
    if (KWindowSystem::isPlatformX11()) {
        wmLog(QStringLiteral("WindowMonitor: X11 platform detected"));
        setupX11();
    } else if (KWindowSystem::isPlatformWayland()) {
        wmLog(QStringLiteral("WindowMonitor: Wayland platform detected"));
        setupWayland();
    } else {
        wmLog(QStringLiteral("WindowMonitor: unknown platform"));
    }
#endif
}

WindowMonitor::~WindowMonitor()
{
}

bool WindowMonitor::hasFullscreenWindow() const
{
#ifdef Q_OS_LINUX
    if (m_isX11) {
        return !m_fullscreenWids.isEmpty();
    }
    return m_hasFullscreen;
#else
    return false;
#endif
}

void WindowMonitor::setFullscreen(bool fs)
{
    if (fs) {
        m_resumeDebounce.stop();
        if (!m_hasFullscreen) {
            m_hasFullscreen = true;
            wmLog(QStringLiteral("WindowMonitor: hasFullscreen changed to 1"));
            emit hasFullscreenWindowChanged();
        }
    } else {
        m_rawFullscreen = false;
        if (m_hasFullscreen && !m_resumeDebounce.isActive()) {
            m_resumeDebounce.start();
        }
    }
}

// --- X11 ---

void WindowMonitor::setupX11()
{
    m_isX11 = true;

    connect(KX11Extras::self(), &KX11Extras::windowAdded,
            this, [this](WId) { rescan(); });
    connect(KX11Extras::self(), &KX11Extras::windowRemoved,
            this, [this](WId) { rescan(); });
    connect(KX11Extras::self(), &KX11Extras::windowChanged,
            this, [this](WId, NET::Properties props, NET::Properties2) {
        if (props & NET::WMState) {
            rescan();
        }
    });
    rescan();
}

void WindowMonitor::rescan()
{
    QSet<quintptr> current;
    const auto windows = KX11Extras::windows();
    for (WId wid : windows) {
        KWindowInfo info(wid, NET::WMState | NET::WMWindowType);
        if (!info.valid()) {
            continue;
        }
        if (info.windowType(NET::AllTypesMask) == NET::Desktop) {
            continue;
        }
        if (info.state() & NET::FullScreen) {
            current.insert(static_cast<quintptr>(wid));
        }
    }
    if (current != m_fullscreenWids) {
        m_fullscreenWids = current;
        emit hasFullscreenWindowChanged();
    }
}

// --- Wayland ---

void WindowMonitor::setupWayland()
{
    m_connection = KWayland::Client::ConnectionThread::fromApplication(this);
    if (!m_connection) {
        wmLog(QStringLiteral("WindowMonitor: fromApplication() returned null"));
        return;
    }
    wmLog(QStringLiteral("WindowMonitor: fromApplication() OK"));

    m_registry = new KWayland::Client::Registry(this);
    connect(m_registry, &KWayland::Client::Registry::plasmaWindowManagementAnnounced,
            this, &WindowMonitor::onRegistryCreated);
    m_registry->create(m_connection);
    m_registry->setup();
    m_connection->roundtrip();
}

void WindowMonitor::onRegistryCreated(quint32 name, quint32 version)
{
    wmLog(QStringLiteral("WindowMonitor: plasmaWindowManagementAnnounced name=%1 version=%2").arg(name).arg(version));

    if (m_management) {
        return;
    }

    m_management = m_registry->createPlasmaWindowManagement(name, version, this);
    if (!m_management) {
        wmLog(QStringLiteral("WindowMonitor: createPlasmaWindowManagement failed"));
        return;
    }

    connect(m_management, &KWayland::Client::PlasmaWindowManagement::windowCreated,
            this, &WindowMonitor::trackWindow);

    const auto existing = m_management->windows();
    wmLog(QStringLiteral("WindowMonitor: existing windows=%1").arg(existing.size()));
    for (auto *w : existing) {
        trackWindow(w);
    }

    recheckAll();
}

void WindowMonitor::trackWindow(KWayland::Client::PlasmaWindow *window)
{
    if (!window || m_windows.contains(window)) {
        return;
    }

    m_windows.insert(window);
    wmLog(QStringLiteral("WindowMonitor: tracking window app=%1 fullscreen=%2")
              .arg(window->appId()).arg(window->isFullscreen()));

    connect(window, &KWayland::Client::PlasmaWindow::fullscreenChanged,
            this, &WindowMonitor::onWindowFullscreenChanged);
    connect(window, &KWayland::Client::PlasmaWindow::unmapped,
            this, [this, window]() {
        m_windows.remove(window);
        wmLog(QStringLiteral("WindowMonitor: window removed app=%1").arg(window->appId()));
        recheckAll();
    });

    if (window->isFullscreen()) {
        recheckAll();
    }
}

void WindowMonitor::recheckAll()
{
    bool rawFs = false;
    for (auto *w : m_windows) {
        if (w->isFullscreen()) {
            wmLog(QStringLiteral("WindowMonitor: fullscreen window: %1").arg(w->appId()));
            rawFs = true;
            break;
        }
    }
    wmLog(QStringLiteral("WindowMonitor: recheckAll tracked=%1 rawFs=%2").arg(m_windows.size()).arg(rawFs));
    m_rawFullscreen = rawFs;
    setFullscreen(rawFs);
}

void WindowMonitor::onWindowFullscreenChanged()
{
    auto *window = qobject_cast<KWayland::Client::PlasmaWindow *>(sender());
    if (window) {
        wmLog(QStringLiteral("WindowMonitor: fullscreenChanged app=%1 isFs=%2")
                  .arg(window->appId()).arg(window->isFullscreen()));
    }
    recheckAll();
}
