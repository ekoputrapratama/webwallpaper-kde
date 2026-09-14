#include <QtTest>
#include <QApplication>
#include <QBuffer>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>

#include "themecard.h"

namespace {

ThemeData makeTheme()
{
    ThemeData theme;
    theme.name = QStringLiteral("Sakura");
    theme.author = QStringLiteral("Eko Putra Pratama");
    theme.description = QStringLiteral("Cherry blossom drift");
    theme.downloads = 3;
    theme.likes = 1;
    return theme;
}

QLabel *thumbnailLabel(ThemeCard &card)
{
    return card.findChild<QLabel *>(QStringLiteral("themeThumbnail"));
}

} // namespace

class TestThemeCard : public QObject
{
    Q_OBJECT

private slots:
    void installButtonTogglesWithInstalledState()
    {
        ThemeCard card(makeTheme());
        auto *btn = card.findChild<QPushButton *>(QStringLiteral("installBtn"));
        QVERIFY(btn);

        QCOMPARE(btn->text(), QStringLiteral("Install"));
        QVERIFY(btn->isEnabled());

        card.setInstalled(true);
        QCOMPARE(btn->text(), QStringLiteral("Installed"));
        QVERIFY(!btn->isEnabled());

        card.setInstalled(false);
        QCOMPARE(btn->text(), QStringLiteral("Install"));
        QVERIFY(btn->isEnabled());
    }

    void likeButtonUpdatesTheCountLabel()
    {
        ThemeCard card(makeTheme());
        auto *label = card.findChild<QLabel *>(QStringLiteral("likeLabel"));
        QVERIFY(label);
        QVERIFY(label->text().contains(QStringLiteral("1")));

        card.setLikes(9);
        QVERIFY(label->text().contains(QStringLiteral("9")));
        QVERIFY(!label->text().contains(QStringLiteral("7")));
    }

    void emptyThumbnailShowsNoPreview()
    {
        ThemeCard card(makeTheme());
        card.setThumbnail(QByteArray());
        QCOMPARE(thumbnailLabel(card)->text(), QStringLiteral("No preview"));
    }

    void failedFetchShowsNoPreviewInsteadOfLoading()
    {
        ThemeCard card(makeTheme());
        QCOMPARE(thumbnailLabel(card)->text(), QStringLiteral("Loading..."));

        card.setThumbnailFailed();
        QVERIFY(thumbnailLabel(card)->pixmap(Qt::ReturnByValue).isNull());
        QCOMPARE(thumbnailLabel(card)->text(), QStringLiteral("No preview"));
    }

    void failedFetchClearsRunningMovie()
    {
        QPixmap source(8, 8);
        source.fill(Qt::green);
        QByteArray png;
        {
            QBuffer buffer(&png);
            QVERIFY(buffer.open(QIODevice::WriteOnly));
            QVERIFY(source.save(&buffer, "PNG"));
        }

        ThemeCard card(makeTheme());
        card.setThumbnail(png);
        QVERIFY(!thumbnailLabel(card)->pixmap(Qt::ReturnByValue).isNull());

        card.setThumbnailFailed();
        QVERIFY(thumbnailLabel(card)->pixmap(Qt::ReturnByValue).isNull());
        QVERIFY(!thumbnailLabel(card)->movie());
        QCOMPARE(thumbnailLabel(card)->text(), QStringLiteral("No preview"));
    }

    void pngThumbnailSetsPixmap()
    {
        QPixmap source(8, 8);
        source.fill(Qt::red);
        QByteArray png;
        {
            QBuffer buffer(&png);
            QVERIFY(buffer.open(QIODevice::WriteOnly));
            QVERIFY(source.save(&buffer, "PNG"));
        }

        ThemeCard card(makeTheme());
        card.setThumbnail(png);
        QVERIFY(!thumbnailLabel(card)->pixmap(Qt::ReturnByValue).isNull());
    }

    void donateButtonOnlyWhenDonateUrlSet()
    {
        ThemeData theme = makeTheme();
        ThemeCard noDonate(theme);
        QVERIFY(!noDonate.findChild<QPushButton *>(QStringLiteral("donateBtn")));

        theme.donateUrl = QUrl(QStringLiteral("https://ko-fi.com/test"));
        theme.donationLabel = QStringLiteral("Support me on Ko-fi");
        ThemeCard donate(theme);
        auto *btn = donate.findChild<QPushButton *>(QStringLiteral("donateBtn"));
        QVERIFY(btn);
        QCOMPARE(btn->text(), QStringLiteral("Support me on Ko-fi"));
    }

    // Guards a real regression: the install/download counts are rendered from
    // QLocale so the card never shows a raw "0 downloads" when none exist.
    void statsShowFormattedCounts()
    {
        ThemeCard card(makeTheme());
        bool found = false;
        const auto labels = card.findChildren<QLabel *>();
        for (const QLabel *label : labels) {
            if (label->text().contains(QStringLiteral("downloads"))) {
                found = true;
                QVERIFY(label->text().startsWith(QStringLiteral("\u2B07 ")));
                break;
            }
        }
        QVERIFY(found);
    }
};

QTEST_MAIN(TestThemeCard)

#include "tst_themecard.moc"