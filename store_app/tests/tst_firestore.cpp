#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>

#include "firestore.h"

// Builds a Firestore `fields` object in the exact wire format the REST API
// returns: every scalar is wrapped in {stringValue}/{integerValue} and arrays
// in {arrayValue:{values:[...]}}.
namespace {

QJsonObject stringField(const QString &value)
{
    return QJsonObject{{QStringLiteral("stringValue"), value}};
}

QJsonObject integerField(qlonglong value)
{
    return QJsonObject{{QStringLiteral("integerValue"), QString::number(value)}};
}

QJsonObject tagFields(const QStringList &tags)
{
    QJsonArray values;
    for (const QString &tag : tags)
        values.append(stringField(tag));
    return QJsonObject{{QStringLiteral("arrayValue"),
                        QJsonObject{{QStringLiteral("values"), values}}}};
}

} // namespace

class TestFirestore : public QObject
{
    Q_OBJECT

private slots:
    void parsesFullDocument()
    {
        QJsonObject fields{
            {QStringLiteral("name"), stringField(QStringLiteral("Sakura"))},
            {QStringLiteral("description"), stringField(QStringLiteral("Cherry blossom drift"))},
            {QStringLiteral("author"), stringField(QStringLiteral("Eko Putra Pratama"))},
            {QStringLiteral("thumbnail_url"), stringField(QStringLiteral("https://example.test/sakura.gif"))},
            {QStringLiteral("wallpaper_url"), stringField(QStringLiteral("https://example.test/sakura.zip"))},
            {QStringLiteral("donation_url"), stringField(QStringLiteral("https://ko-fi.com/test"))},
            {QStringLiteral("downloads"), integerField(12)},
            {QStringLiteral("likes"), integerField(7)},
            {QStringLiteral("uid"), stringField(QStringLiteral("u123"))},
            {QStringLiteral("tags"), tagFields({QStringLiteral("animated"), QStringLiteral("webgl")})},
        };

        const ThemeData theme = parseTheme(fields);

        QCOMPARE(theme.name, QStringLiteral("Sakura"));
        QCOMPARE(theme.description, QStringLiteral("Cherry blossom drift"));
        QCOMPARE(theme.author, QStringLiteral("Eko Putra Pratama"));
        QCOMPARE(theme.thumbnailUrl, QUrl(QStringLiteral("https://example.test/sakura.gif")));
        QCOMPARE(theme.downloadUrl, QUrl(QStringLiteral("https://example.test/sakura.zip")));
        QCOMPARE(theme.donateUrl, QUrl(QStringLiteral("https://ko-fi.com/test")));
        QCOMPARE(theme.downloads, qint64(12));
        QCOMPARE(theme.likes, qint64(7));
        QCOMPARE(theme.uid, QStringLiteral("u123"));
        QCOMPARE(theme.tags, QStringList({QStringLiteral("animated"), QStringLiteral("webgl")}));
        QVERIFY(theme.isValid());
    }

    void emptyDescriptionAndInstallCountsAreOptional()
    {
        // Published themes may omit description/author/tags; the thumbnail can
        // be an SVG and stats may be absent entirely.
        QJsonObject fields{
            {QStringLiteral("name"), stringField(QStringLiteral("Ocean Waves"))},
            {QStringLiteral("thumbnail_url"), stringField(QStringLiteral("https://example.test/waves.svg"))},
            {QStringLiteral("wallpaper_url"), stringField(QStringLiteral("https://example.test/waves.zip"))},
        };

        const ThemeData theme = parseTheme(fields);

        QVERIFY(theme.description.isEmpty());
        QVERIFY(theme.author.isEmpty());
        QVERIFY(theme.tags.isEmpty());
        QCOMPARE(theme.downloads, qint64(0));
        QCOMPARE(theme.likes, qint64(0));
        QVERIFY(theme.isValid());
    }

    void invalidWithoutNameOrWallpaperUrl()
    {
        QJsonObject noName{{QStringLiteral("wallpaper_url"),
                            stringField(QStringLiteral("https://example.test/a.zip"))}};
        QVERIFY(!parseTheme(noName).isValid());

        // Stale/placeholder docs (name present but nothing downloadable).
        QJsonObject noFont{
            {QStringLiteral("name"), stringField(QStringLiteral("Ghost"))},
            {QStringLiteral("thumbnail_url"), stringField(QStringLiteral("https://example.test/x.gif"))},
        };
        QVERIFY(!parseTheme(noFont).isValid());

        // Not even a fields object (malformed row).
        QVERIFY(!parseTheme(QJsonObject{}).isValid());
    }

    void slugify_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");

        QTest::newRow("space") << "Rotating Rubix" << "rotating-rubix";
        QTest::newRow("multiple spaces") << "KDE Plasma Shader" << "kde-plasma-shader";
        QTest::newRow("camel case") << "OceanWaves" << "oceanwaves";
        QTest::newRow("punctuation") << "Sakura -- Cherry Blossom!" << "sakura-cherry-blossom";
        QTest::newRow("digits kept") << "Shader 2.0" << "shader-2-0";
        QTest::newRow("empty") << "" << "theme";
        QTest::newRow("symbols only") << "!!! +++" << "theme";
    }

    void slugify()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(::slugify(input), expected);
    }
};

QTEST_GUILESS_MAIN(TestFirestore)

#include "tst_firestore.moc"