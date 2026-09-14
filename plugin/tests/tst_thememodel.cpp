#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QScopedPointer>
#include <QTextStream>

#include "thememodel.h"

namespace {

void writeFile(const QString &path, const QString &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        qFatal("Failed to create %s: %s", qPrintable(path), qPrintable(f.errorString()));
    f.write(contents.toUtf8());
    f.close();
}

QModelIndex rowForName(ThemeModel &model, const QString &name)
{
    for (int i = 0; i < model.rowCount(); ++i) {
        const QModelIndex idx = model.index(i, 0);
        if (model.data(idx, ThemeModel::NameRole).toString() == name)
            return idx;
    }
    return {};
}

} // namespace

class TestThemeModel : public QObject
{
    Q_OBJECT

    QScopedPointer<QTemporaryDir> m_dir;
    QString m_fixtureDir;
    QString m_minimalDir;

private slots:
    void initTestCase()
    {
        m_dir.reset(new QTemporaryDir);
        QVERIFY(m_dir->isValid());
        m_fixtureDir = m_dir->path() + QStringLiteral("/fixture");
        m_minimalDir = m_dir->path() + QStringLiteral("/minimal");

        // demo: lowercase .theme keys (one of the two conventions in the wild)
        writeFile(m_fixtureDir + QStringLiteral("/demo/thumbnail.png"), QString());
        writeFile(m_fixtureDir + QStringLiteral("/demo/index.html"), QString());
        writeFile(m_fixtureDir + QStringLiteral("/demo/demo.theme"),
                  QStringLiteral("[Theme]\n"
                                 "name=demo\n"
                                 "description=Animated starfield\n"
                                 "entry=index.html\n"
                                 "thumbnail=thumbnail.png\n"));

        // sakura: capitalized .theme keys (the canonical store-app form).
        // Regression guard for the case-SENSITIVE QSettings key bug (Session #9).
        writeFile(m_fixtureDir + QStringLiteral("/sakura/thumbnail.svg"), QString());
        writeFile(m_fixtureDir + QStringLiteral("/sakura/index.html"), QString());
        writeFile(m_fixtureDir + QStringLiteral("/sakura/sakura.theme"),
                  QStringLiteral("[Theme]\n"
                                 "Name=Sakura\n"
                                 "Description=Cherry blossom drift\n"
                                 "Author=Eko Putra Pratama\n"
                                 "Thumbnail=thumbnail.svg\n"
                                 "Entry=index.html\n"));

        // duplicate: two .theme files in the SAME dir resolve to the same
        // baseDir -> must be deduplicated to a single row.
        writeFile(m_fixtureDir + QStringLiteral("/dupe/index.html"), QString());
        writeFile(m_fixtureDir + QStringLiteral("/dupe/a.theme"),
                  QStringLiteral("[Theme]\nName=X\nEntry=index.html\n"));
        writeFile(m_fixtureDir + QStringLiteral("/dupe/b.theme"),
                  QStringLiteral("[Theme]\nName=X\nEntry=index.html\n"));

        // broken: entry missing -> must be skipped entirely.
        writeFile(m_fixtureDir + QStringLiteral("/broken/broken.theme"),
                  QStringLiteral("[Theme]\nName=Broken\n"));

        qputenv("WEBWALLPAPER_THEMES_DIR", m_fixtureDir.toUtf8());

        // A tiny second fixture with no thumbnail key, for isolated checks.
        writeFile(m_minimalDir + QStringLiteral("/nopreview/index.html"), QString());
        writeFile(m_minimalDir + QStringLiteral("/nopreview/nopreview.theme"),
                  QStringLiteral("[Theme]\nName=No Preview\nEntry=index.html\n"));
    }

    void cleanupTestCase()
    {
        qunsetenv("WEBWALLPAPER_THEMES_DIR");
        m_dir.reset();
    }

    void scansValidThemesAndSkipsBroken()
    {
        ThemeModel model;

        // demo + sakura + dupe(a,b deduped) = 3; broken skipped.
        QCOMPARE(model.rowCount(), 3);

        QVERIFY(rowForName(model, QStringLiteral("demo")).isValid());
        QVERIFY(rowForName(model, QStringLiteral("Sakura")).isValid());
        QCOMPARE(rowForName(model, QStringLiteral("Broken")).isValid(), false);
    }

    void acceptsBothKeyCasing()
    {
        ThemeModel model;

        const QModelIndex demo = rowForName(model, QStringLiteral("demo"));
        const QModelIndex sakura = rowForName(model, QStringLiteral("Sakura"));
        QVERIFY(demo.isValid());
        QVERIFY(sakura.isValid());

        QCOMPARE(model.data(demo, ThemeModel::DescriptionRole).toString(),
                 QStringLiteral("Animated starfield"));
        QCOMPARE(model.data(sakura, ThemeModel::DescriptionRole).toString(),
                 QStringLiteral("Cherry blossom drift"));
        QCOMPARE(model.data(sakura, ThemeModel::EntryRole).toString(),
                 QStringLiteral("file://") + m_fixtureDir + QStringLiteral("/sakura/index.html"));
    }

    void previewIsFileUrl()
    {
        ThemeModel model;
        const QModelIndex sakura = rowForName(model, QStringLiteral("Sakura"));
        QVERIFY(sakura.isValid());

        const QString preview = model.data(sakura, ThemeModel::PreviewRole).toString();
        QVERIFY(preview.startsWith(QStringLiteral("file://")));
        QVERIFY(preview.endsWith(QStringLiteral("/sakura/thumbnail.svg")));
    }

    void pathRolePointsAtThemeDir()
    {
        ThemeModel model;
        const QModelIndex demo = rowForName(model, QStringLiteral("demo"));
        QVERIFY(demo.isValid());
        QCOMPARE(model.data(demo, ThemeModel::PathRole).toString(),
                 m_fixtureDir + QStringLiteral("/demo"));
    }

    void defaultThemePicksDemoByName()
    {
        ThemeModel model;

        QVERIFY(!model.defaultThemePath().isEmpty());
        QCOMPARE(model.defaultThemePath(), m_fixtureDir + QStringLiteral("/demo"));
    }

    void missingThumbnailYieldsEmptyPreviewAndFirstThemeDefault()
    {
        qputenv("WEBWALLPAPER_THEMES_DIR", m_minimalDir.toUtf8());
        ThemeModel model;

        QCOMPARE(model.rowCount(), 1);
        const QModelIndex idx = model.index(0, 0);
        QVERIFY(rowForName(model, QStringLiteral("No Preview")).isValid());
        QVERIFY(model.data(idx, ThemeModel::PreviewRole).toString().isEmpty());
        QVERIFY(!model.data(idx, ThemeModel::EntryRole).toString().isEmpty());

        // No theme named "demo" here -> default falls back to the first theme.
        QCOMPARE(model.defaultThemePath(), m_minimalDir + QStringLiteral("/nopreview"));

        qputenv("WEBWALLPAPER_THEMES_DIR", m_fixtureDir.toUtf8());
    }

    void emptyCatalogHasNoDefault()
    {
        const QString emptyDir = m_dir->path() + QStringLiteral("/empty");
        QDir().mkpath(emptyDir);
        qputenv("WEBWALLPAPER_THEMES_DIR", emptyDir.toUtf8());
        ThemeModel model;

        QCOMPARE(model.rowCount(), 0);
        QVERIFY(model.defaultThemePath().isEmpty());

        qputenv("WEBWALLPAPER_THEMES_DIR", m_fixtureDir.toUtf8());
    }

    void roleNamesAreExposed()
    {
        ThemeModel model;
        const auto names = model.roleNames();
        QCOMPARE(names.value(ThemeModel::NameRole), QByteArray("name"));
        QCOMPARE(names.value(ThemeModel::PathRole), QByteArray("path"));
        QCOMPARE(names.value(ThemeModel::PreviewRole), QByteArray("preview"));
        QCOMPARE(names.value(ThemeModel::EntryRole), QByteArray("entry"));
    }
};

QTEST_GUILESS_MAIN(TestThemeModel)

#include "tst_thememodel.moc"