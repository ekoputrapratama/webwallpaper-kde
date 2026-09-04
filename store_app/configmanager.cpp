#include "configmanager.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QResource>
#include <QDebug>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_env()
{
    QCoreApplication::setOrganizationName("WebWallpaper");
    QCoreApplication::setApplicationName("WebWallpaper");
}

QString ConfigManager::lastThemeName() const
{
    return m_settings.value("Theme/lastUsed", "demo").toString();
}

void ConfigManager::setLastThemeName(const QString &name)
{
    m_settings.setValue("Theme/lastUsed", name);
    emit settingsChanged();
}

bool ConfigManager::autoStart() const
{
    return m_settings.value("General/autoStart", false).toBool();
}

void ConfigManager::setAutoStart(bool enabled)
{
    m_settings.setValue("General/autoStart", enabled);
    emit settingsChanged();
}

void ConfigManager::loadDotEnv()
{
    m_env.clear();

    // Search for the .env file in the conventional XDG app config dir,
    // then in the current working directory.
    QStringList candidatePaths = {
        QDir::homePath() + "/.config/webwallpaper/.env",
        QDir::homePath() + "/.config/webwallpaper.env",
        QDir::currentPath() + "/.env",
    };

    QString envPath;
    for (const QString &path : candidatePaths) {
        if (QFileInfo::exists(path)) {
            envPath = path;
            break;
        }
    }

    if (envPath.isEmpty())
        return;

    QFile file(envPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open .env:" << envPath;
        return;
    }

    QTextStream in(&file);
    QRegularExpression lineRe(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)\s*$)");

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        QRegularExpressionMatch match = lineRe.match(line);
        if (!match.hasMatch())
            continue;

        QString key = match.captured(1);
        QString value = match.captured(2);

        if (value.startsWith('"') && value.endsWith('"') && value.size() >= 2)
            value = value.mid(1, value.size() - 2);
        else if (value.startsWith('\'') && value.endsWith('\'') && value.size() >= 2)
            value = value.mid(1, value.size() - 2);

        m_env.insert(key, value);
    }
}

QString ConfigManager::env(const QString &key, const QString &defaultValue) const
{
    QString v = m_env.value(key);
    if (!v.isEmpty())
        return v;

    // Fall back to the configuration embedded in the binary (resources/config/config.env).
    // Keeping credentials out of source but still shipping a working app.
    QResource configRes(QStringLiteral(":/config/config.env"));
    if (configRes.isValid()) {
        QByteArray data(reinterpret_cast<const char *>(configRes.data()),
                        static_cast<int>(configRes.size()));
        QTextStream in(&data);
        QRegularExpression re(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)\s*$)");
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            QRegularExpressionMatch m = re.match(line);
            if (m.hasMatch() && m.captured(1) == key) {
                QString val = m.captured(2);
                if (val.startsWith('"') && val.endsWith('"') && val.size() >= 2)
                    val = val.mid(1, val.size() - 2);
                else if (val.startsWith('\'') && val.endsWith('\'') && val.size() >= 2)
                    val = val.mid(1, val.size() - 2);
                return val;
            }
        }
    }

    return defaultValue;
}

QString ConfigManager::firebaseApiKey() const
{
    return env("FIREBASE_API_KEY", env("FIRESTORE_API_KEY", QString()));
}

QString ConfigManager::firebaseProjectId() const
{
    return env("FIREBASE_PROJECT_ID", env("PROJECT_ID", QString()));
}

QString ConfigManager::firebaseDatabaseId() const
{
    return env("FIREBASE_DATABASE_ID", env("DATABASE_ID", QString()));
}

QString ConfigManager::themesSystemDir() const
{
    return "/usr/share/webwallpaper/themes";
}

QString ConfigManager::themesUserDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + "/webwallpaper/themes";
}
