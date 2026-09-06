#include "thememodel.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>
#include <utility>

ThemeModel::ThemeModel(QObject *parent)
    : QAbstractListModel(parent)
{
    scanThemes();
}

int ThemeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_themes.size();
}

QVariant ThemeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_themes.size())
        return {};

    const Theme &theme = m_themes.at(index.row());

    switch (role) {
    case NameRole:
        return theme.name;
    case DescriptionRole:
        return theme.description;
    case PathRole:
        return theme.path;
    case PreviewRole:
        return theme.preview;
    case EntryRole:
        return theme.entry;
    default:
        return {};
    }
}

QHash<int, QByteArray> ThemeModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { DescriptionRole, "description" },
        { PathRole, "path" },
        { PreviewRole, "preview" },
        { EntryRole, "entry" },
    };
}

QString ThemeModel::defaultThemePath() const
{
    return m_defaultThemePath;
}

void ThemeModel::scanThemes()
{
    beginResetModel();
    m_themes.clear();

    // Theme dirs populated by cmake/deb installs and by the store app.
    const QStringList dirs = {
        QStringLiteral("/usr/share/webwallpaper/themes"),
        QDir::homePath() + QStringLiteral("/.local/share/webwallpaper/themes"),
    };

    for (const QString &dirPath : dirs)
        scanDir(dirPath);

    // Fallback: the demo theme bundled INSIDE the wallpaper package itself
    // (plugin/package/contents/themes). This covers the KDE Store / bare
    // wallpaper-package install where the standalone themes dirs above are
    // never created. Only consulted when the main dirs yielded nothing, so the
    // bundled copy never shows up twice next to a normally-installed one.
    if (m_themes.isEmpty()) {
        const QString pluginId = QStringLiteral("webwallpaper");
        const QStringList packageDirs = {
            QDir::homePath() + QStringLiteral("/.local/share/plasma/wallpapers/") + pluginId,
            QDir::homePath() + QStringLiteral("/.local/share/wallpapers/") + pluginId,
            QStringLiteral("/usr/share/plasma/wallpapers/") + pluginId,
            QStringLiteral("/usr/share/wallpapers/") + pluginId,
        };
        for (const QString &dirPath : packageDirs)
            scanDir(dirPath + QStringLiteral("/contents/themes"));
    }

    // A sane default for "nothing configured yet": prefer the bundled demo
    // theme if present, otherwise the first available theme.
    const QString oldDefault = m_defaultThemePath;
    m_defaultThemePath.clear();
    for (const Theme &theme : std::as_const(m_themes)) {
        if (theme.name.compare(QStringLiteral("demo"), Qt::CaseInsensitive) == 0) {
            m_defaultThemePath = theme.path;
            break;
        }
    }
    if (m_defaultThemePath.isEmpty() && !m_themes.isEmpty())
        m_defaultThemePath = m_themes.constFirst().path;
    if (m_defaultThemePath != oldDefault)
        Q_EMIT defaultThemePathChanged();

    endResetModel();
}

void ThemeModel::scanDir(const QString &dirPath)
{
    if (dirPath.isEmpty())
        return;
    QDir dir(dirPath);
    if (!dir.exists())
        return;

    QDirIterator it(dirPath, { QStringLiteral("*.theme") }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        Theme theme;

        const QString filePath = it.next();
        QSettings settings(filePath, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("Theme"));

        // .theme keys are case-sensitive in QSettings; some files use the
        // canonical capitalized form (Name, Description, Thumbnail, Entry),
        // others lowercase (name, ...). Read the capitalized form first so
        // both conventions are accepted.
        const auto readKey = [&settings](const QString &capitalized,
                                         const QString &lowercase) {
            auto v = settings.value(capitalized);
            if (v.isNull())
                v = settings.value(lowercase);
            return v.toString();
        };
        theme.name = readKey(QStringLiteral("Name"), QStringLiteral("name"));
        theme.description = readKey(QStringLiteral("Description"), QStringLiteral("description"));
        const QString thumbnail = readKey(QStringLiteral("Thumbnail"), QStringLiteral("thumbnail"));
        const QString entry = readKey(QStringLiteral("Entry"), QStringLiteral("entry"));
        settings.endGroup();

        if (theme.name.isEmpty() || entry.isEmpty())
            continue;

        const QString baseDir = QFileInfo(filePath).absoluteDir().absolutePath();
        theme.path = baseDir;

        const auto toFileUrl = [&baseDir](const QString &value) {
            if (value.isEmpty())
                return QString();
            if (value.startsWith(QStringLiteral("file://")))
                return value;
            if (value.startsWith(QLatin1Char('/')))
                return QStringLiteral("file://") + value;
            return QStringLiteral("file://") + baseDir + QLatin1Char('/') + value;
        };

        theme.preview = toFileUrl(thumbnail);
        theme.entry = toFileUrl(entry);

        bool duplicate = false;
        for (const Theme &other : std::as_const(m_themes)) {
            if (other.path == theme.path) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            m_themes.append(theme);
    }
}