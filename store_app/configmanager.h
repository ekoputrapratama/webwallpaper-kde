#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QHash>

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);

    QString lastThemeName() const;
    void setLastThemeName(const QString &name);

    bool autoStart() const;
    void setAutoStart(bool enabled);

    QString firebaseApiKey() const;
    QString firebaseProjectId() const;
    QString firebaseDatabaseId() const;

    void loadDotEnv();
    QString env(const QString &key, const QString &defaultValue = {}) const;

    QString themesSystemDir() const;
    QString themesUserDir() const;

signals:
    void settingsChanged();

private:
    QSettings m_settings;
    QHash<QString, QString> m_env;
};
