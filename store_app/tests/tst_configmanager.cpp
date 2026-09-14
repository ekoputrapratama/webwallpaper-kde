#include <QtTest>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTextStream>

#include "configmanager.h"

class TestConfigManager : public QObject
{
    Q_OBJECT

    QScopedPointer<QTemporaryDir> m_dir;

    static void writeEnvFile(const QString &path, const QString &contents)
    {
        QFile f(path);
        Q_ASSERT(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(contents.toUtf8());
        f.close();
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("WebWallpaperTest"));
        QCoreApplication::setApplicationName(QStringLiteral("WebWallpaperTest"));
        // Keep QSettings/QStandardPaths writes out of the real user profile.
        QStandardPaths::setTestModeEnabled(true);

        m_dir.reset(new QTemporaryDir);
        QVERIFY(m_dir->isValid());

        // loadDotEnv() searches <cwd>/.env; point it at our scratch dir.
        writeEnvFile(QDir::currentPath() + QStringLiteral("/.env"), QStringLiteral());
        QVERIFY(QDir::setCurrent(m_dir->path()));

        writeEnvFile(m_dir->path() + QStringLiteral("/.env"),
                     QStringLiteral("# comment line\n"
                                    "\n"
                                    "SIMPLE=value\n"
                                    "QUOTED=\"a b c d\"\n"
                                    "SINGLE='x=1 y=2'\n"
                                    "   PADDED   =   spaced   \n"
                                    "FIREBASE_API_KEY=test-api-key-123\n"
                                    "FIREBASE_PROJECT_ID=test-project\n"
                                    "FIREBASE_DATABASE_ID=test-database\n"));
    }

    void cleanupTestCase()
    {
        QDir::setCurrent(QDir::homePath());
        writeEnvFile(QDir::homePath() + QStringLiteral("/.env"), QStringLiteral());
        m_dir.reset();
    }

    void parsesDotEnv()
    {
        ConfigManager config;
        config.loadDotEnv();

        QCOMPARE(config.env(QStringLiteral("SIMPLE")), QStringLiteral("value"));
        QCOMPARE(config.env(QStringLiteral("QUOTED")), QStringLiteral("a b c d"));
        QCOMPARE(config.env(QStringLiteral("SINGLE")), QStringLiteral("x=1 y=2"));
        QCOMPARE(config.env(QStringLiteral("PADDED")), QStringLiteral("spaced"));
    }

    void missingKeysReturnDefault()
    {
        ConfigManager config;
        config.loadDotEnv();

        QCOMPARE(config.env(QStringLiteral("DOES_NOT_EXIST")), QString());
        QCOMPARE(config.env(QStringLiteral("DOES_NOT_EXIST"), QStringLiteral("fallback")),
                 QStringLiteral("fallback"));
    }

    void firebaseGetters()
    {
        ConfigManager config;
        config.loadDotEnv();

        QCOMPARE(config.firebaseApiKey(), QStringLiteral("test-api-key-123"));
        QCOMPARE(config.firebaseProjectId(), QStringLiteral("test-project"));
        QCOMPARE(config.firebaseDatabaseId(), QStringLiteral("test-database"));
    }

    // FIREBASE_* wins over the legacy FIRESTORE_* spelling when both are set.
    void firestoreAliasUsedWhenFirebaseKeyMissing()
    {
        writeEnvFile(m_dir->path() + QStringLiteral("/.env"),
                     QStringLiteral("FIRESTORE_API_KEY=legacy-key\n"
                                    "PROJECT_ID=legacy-project\n"
                                    "DATABASE_ID=legacy-db\n"));

        ConfigManager config;
        config.loadDotEnv();

        QCOMPARE(config.firebaseApiKey(), QStringLiteral("legacy-key"));
        QCOMPARE(config.firebaseProjectId(), QStringLiteral("legacy-project"));
        QCOMPARE(config.firebaseDatabaseId(), QStringLiteral("legacy-db"));

        // Restore the primary .env for the remaining tests.
        writeEnvFile(m_dir->path() + QStringLiteral("/.env"),
                     QStringLiteral("FIREBASE_API_KEY=test-api-key-123\n"
                                    "FIREBASE_PROJECT_ID=test-project\n"
                                    "FIREBASE_DATABASE_ID=test-database\n"));
    }

    void settingsRoundTrip()
    {
        ConfigManager config;

        QSignalSpy spy(&config, &ConfigManager::settingsChanged);
        config.setLastThemeName(QStringLiteral("sakura"));
        QCOMPARE(config.lastThemeName(), QStringLiteral("sakura"));
        QCOMPARE(spy.count(), 1);

        config.setAutoStart(true);
        QVERIFY(config.autoStart());
        QCOMPARE(spy.count(), 2);
    }

    void themeDirs()
    {
        ConfigManager config;
        QVERIFY(config.themesUserDir().endsWith(QStringLiteral("/webwallpaper/themes")));
        QCOMPARE(config.themesSystemDir(), QStringLiteral("/usr/share/webwallpaper/themes"));
    }
};

QTEST_MAIN(TestConfigManager)

#include "tst_configmanager.moc"