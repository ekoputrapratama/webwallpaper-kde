#include "storewindow.h"
#include "networkmanager.h"
#include "configmanager.h"
#include "themecard.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QDesktopServices>
#include <QJsonValue>
#include <QRegularExpression>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QApplication>

StoreWindow::StoreWindow(NetworkManager *netMgr, ConfigManager *config, QWidget *parent)
    : QDialog(parent)
    , m_netMgr(netMgr)
    , m_config(config)
{
    setWindowTitle("Web Wallpaper Store");
    resize(860, 620);
    setStyleSheet(
        "QDialog { background: #1c1c21; }"
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: #1c1c21; }"
        "#themeCard { background: #26262c; border: 1px solid #33333a; border-radius: 10px; }"
        "#themeCard:hover { border: 1px solid #3d7eff; }"
        "#themeBody { background: transparent; }"
        "QProgressBar { text-align: center; color: #f2f2f4; background: #26262c; border: 1px solid #33333a; border-radius: 4px; }"
        "QScrollBar:vertical { background: #1c1c21; width: 10px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #44444c; border-radius: 5px; min-height: 40px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }");

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(10);

    auto *header = new QHBoxLayout();
    auto *titleLabel = new QLabel("Web Wallpaper Store", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: 700; color: #f2f2f4;");
    header->addWidget(titleLabel);
    header->addStretch();

    m_wallpaperSettingsBtn = new QPushButton("Wallpaper Settings", this);
    m_wallpaperSettingsBtn->setCursor(Qt::PointingHandCursor);
    m_wallpaperSettingsBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #c0c0c6; border: 1px solid #44444c;"
        " border-radius: 6px; font-weight: 600; padding: 7px 14px; }"
        "QPushButton:hover { color: #f2f2f4; border-color: #7aa2f7; }");
    header->addWidget(m_wallpaperSettingsBtn);

    m_refreshBtn = new QPushButton("Refresh", this);
    header->addWidget(m_refreshBtn);
    outer->addLayout(header);

    m_statusLabel = new QLabel("Loading themes...", this);
    m_statusLabel->setStyleSheet("color: #b0b0b8;");
    outer->addWidget(m_statusLabel);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_gridHost = new QWidget();
    m_gridHost->setObjectName("gridHost");

    auto *hostV = new QVBoxLayout(m_gridHost);
    hostV->setContentsMargins(4, 4, 4, 4);
    hostV->setSpacing(0);

    m_grid = new QGridLayout();
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(16);
    m_grid->setVerticalSpacing(16);
    m_grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    hostV->addLayout(m_grid);

    m_loadingMoreLabel = new QLabel(QStringLiteral("Loading more..."), m_gridHost);
    m_loadingMoreLabel->setAlignment(Qt::AlignCenter);
    m_loadingMoreLabel->setStyleSheet("color: #9a9aa2; padding: 16px; font-weight: 600;");
    m_loadingMoreLabel->setVisible(false);
    hostV->addWidget(m_loadingMoreLabel);

    m_layoutHint = new QLabel("No themes available yet.", m_gridHost);
    m_layoutHint->setAlignment(Qt::AlignCenter);
    m_layoutHint->setStyleSheet("color: #8b8b90; padding: 40px;");
    m_layoutHint->setVisible(false);
    m_grid->addWidget(m_layoutHint, 0, 0, 1, m_columns);

    m_scrollArea->setWidget(m_gridHost);
    outer->addWidget(m_scrollArea, 1);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumHeight(18);
    outer->addWidget(m_progressBar);

    connect(m_refreshBtn, &QPushButton::clicked, this, &StoreWindow::onFetchThemes);
    connect(m_wallpaperSettingsBtn, &QPushButton::clicked, this, []() {
        // Guarantee the WebWallpaper QML module is importable by the child,
        // regardless of how the store itself was launched (could be from a
        // shell without QML2_IMPORT_PATH). The module lives in the user-local
        // Qt QML dir for a --user local install; a --system install resolves
        // it from the default paths, so this extra path is harmless there.
        auto *proc = new QProcess;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        const QString userQml = QDir::homePath() + QStringLiteral("/.local/lib/qt6/qml");
        const QString existing = env.value(QStringLiteral("QML2_IMPORT_PATH"));
        const QString importPath = existing.isEmpty() ? userQml : existing + QLatin1Char(':') + userQml;
        env.insert(QStringLiteral("QML2_IMPORT_PATH"), importPath);
        proc->setProcessEnvironment(env);
        proc->setProgram(QStringLiteral("systemsettings"));
        proc->setArguments({ QStringLiteral("kcm_wallpaper") });
        proc->setParent(qApp);

        // Detach and release the QProcess once the child is running.
        QObject::connect(proc, &QProcess::started, proc, &QObject::deleteLater);
        proc->startDetached();
        delete proc;
    });
    connect(m_netMgr, &NetworkManager::themePageFetched,
            this, &StoreWindow::onThemePageReceived);
    connect(m_netMgr, &NetworkManager::fetchError,
            this, &StoreWindow::onFetchError);
    connect(m_netMgr, &NetworkManager::imageFetched,
            this, &StoreWindow::onImageFetched);
    connect(m_netMgr, &NetworkManager::downloadProgress,
            this, &StoreWindow::onDownloadProgress);
    connect(m_netMgr, &NetworkManager::downloadFinished,
            this, &StoreWindow::onDownloadFinished);
    connect(m_netMgr, &NetworkManager::downloadError,
            this, &StoreWindow::onDownloadError);

    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::rangeChanged,
            this, &StoreWindow::onScrollRangeChanged);

    onFetchThemes();
}

ThemeData StoreWindow::parseTheme(const QJsonObject &fields) const
{
    ThemeData theme;
    const auto str = [&fields](const char *key) -> QString {
        return fields[key].toObject()["stringValue"].toString();
    };
    theme.name = str("name");
    theme.description = str("description");
    theme.author = str("author");
    theme.thumbnailUrl = QUrl(str("thumbnail_url"));
    theme.downloadUrl = QUrl(str("wallpaper_url"));
    theme.donateUrl = QUrl(str("donation_url"));
    theme.donationLabel = str("donation_label");
    theme.uid = str("uid");
    theme.downloads = fields["downloads"].toObject()["integerValue"].toString().toLongLong();
    theme.likes = fields["likes"].toObject()["integerValue"].toString().toLongLong();

    const QJsonArray tags = fields["tags"].toObject()["arrayValue"].toObject()["values"].toArray();
    for (const QJsonValue &tag : tags)
        theme.tags << tag.toObject()["stringValue"].toString();

    return theme;
}

static QString slugify(const QString &name)
{
    QString slug = name.toLower();
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    slug.remove(QRegularExpression(QStringLiteral("^-|-$")));
    return slug.isEmpty() ? QStringLiteral("theme") : slug;
}

void StoreWindow::onFetchThemes()
{
    QString apiKey = m_config->firebaseApiKey();
    QString projectId = m_config->firebaseProjectId();
    QString databaseId = m_config->firebaseDatabaseId();

    if (apiKey.isEmpty() || projectId.isEmpty()) {
        m_statusLabel->setText("Not configured. Create ~/.config/webwallpaper/.env with FIREBASE_API_KEY and FIREBASE_PROJECT_ID.");
        return;
    }

    setBusy(true);
    m_statusLabel->setText("Fetching themes...");
    clearGrid();
    m_nextPageToken.clear();
    m_hasMorePages = false;
    m_loadingMore = false;
    m_page = 0;
    m_themeIdByThumbUrl.clear();
    m_themeDocPath.clear();
    m_netMgr->fetchThemeList(apiKey, projectId, databaseId, m_pageSize);
}

void StoreWindow::onThemePageReceived(const QJsonDocument &doc, const QString &nextPageToken)
{
    setBusy(false);

    const QJsonArray documents = doc["documents"].toArray();
    const int prevCount = m_cards.size();

    for (const auto &val : documents) {
        QJsonObject obj = val.toObject();
        QJsonObject fields = obj["fields"].toObject();

        ThemeData theme = parseTheme(fields);
        if (!theme.isValid())
            continue;

        QString themeId = slugify(theme.name);
        if (themeId.isEmpty())
            themeId = theme.name;

        m_themeDocPath.insert(themeId, obj["name"].toString());

        if (theme.thumbnailUrl.isValid())
            m_themeIdByThumbUrl.insert(theme.thumbnailUrl, themeId);

        addCard(themeId, theme);
    }

    m_nextPageToken = nextPageToken;
    m_hasMorePages = !nextPageToken.isEmpty();
    m_page++;

    // Refresh the bottom indicator: visible "Loading more..." while there's a
    // next page, "End of list" when exhausted. It stays hidden if content
    // doesn't fill the viewport yet (auto-load happens below).
    m_loadingMore = false;
    if (m_loadingMoreLabel) {
        bool exhausted = !m_hasMorePages && !m_cards.isEmpty();
        m_loadingMoreLabel->setVisible(exhausted);
        if (exhausted)
            m_loadingMoreLabel->setText(QStringLiteral("End of list"));
    }

    // Fetch thumbnails for new cards only
    for (int i = prevCount; i < m_cards.size(); ++i) {
        if (m_cards[i]->themeData().thumbnailUrl.isValid())
            m_netMgr->fetchImage(m_cards[i]->themeData().thumbnailUrl);
    }

    if (prevCount == 0 && m_cards.isEmpty()) {
        m_statusLabel->setText(QString("No themes published yet (%1 docs scanned).").arg(documents.size()));
        m_layoutHint->setVisible(true);
    } else {
        m_statusLabel->setText(QStringLiteral("%1 themes loaded.").arg(m_cards.size()));
        m_layoutHint->setVisible(false);
    }

    if (m_hasMorePages && shouldLoadMore())
        fetchNextPage();
}

void StoreWindow::onFetchError(const QString &error)
{
    setBusy(false);
    m_statusLabel->setText("Error: " + error);
}

void StoreWindow::onImageFetched(const QUrl &url, const QByteArray &data)
{
    QString id = m_themeIdByThumbUrl.value(url);
    if (id.isEmpty())
        return;

    for (ThemeCard *card : m_cards) {
        if (card->themeId() == id) {
            card->setThumbnail(data);
            return;
        }
    }
}

void StoreWindow::onInstallClicked(const QString &themeId, const ThemeData &theme)
{
    m_selectedThemeId = themeId.isEmpty() ? theme.name : themeId;
    QString destDir = m_config->themesUserDir() + "/" + m_selectedThemeId;

    setBusy(true);
    m_statusLabel->setText("Downloading " + theme.name + "...");
    m_netMgr->downloadTheme(theme.downloadUrl.toString(), destDir, m_selectedThemeId);
}

void StoreWindow::onLikeClicked(const QString &themeId, const ThemeData &theme)
{
    // Optimistically bump the visible count, then persist the increment.
    for (ThemeCard *card : m_cards) {
        if (card->themeId() == themeId) {
            card->setLikes(card->themeData().likes + 1);
            break;
        }
    }

    const QString documentPath = m_themeDocPath.value(themeId.isEmpty() ? theme.name : themeId);
    if (!documentPath.isEmpty())
        m_netMgr->incrementLikes(documentPath,
                                 m_config->firebaseApiKey(),
                                 m_config->firebaseProjectId(),
                                 m_config->firebaseDatabaseId());
}

void StoreWindow::onDownloadProgress(qint64 received, qint64 total)
{
    m_progressBar->setVisible(true);
    if (total > 0) {
        m_progressBar->setMaximum(total);
        m_progressBar->setValue(received);
    } else {
        m_progressBar->setRange(0, 0);
    }
}

void StoreWindow::onDownloadFinished(const QString &themeDir)
{
    setBusy(false);
    m_progressBar->setVisible(false);
    m_statusLabel->setText(m_selectedThemeId + " installed to " + themeDir);

    for (ThemeCard *card : m_cards) {
        if (card->themeId() == m_selectedThemeId) {
            card->setInstalled(true);
            break;
        }
    }

    // Bump the Firestore download count for the installed theme (fire-and-forget).
    const QString documentPath = m_themeDocPath.value(m_selectedThemeId);
    if (!documentPath.isEmpty())
        m_netMgr->incrementDownload(documentPath,
                                    m_config->firebaseApiKey(),
                                    m_config->firebaseProjectId(),
                                    m_config->firebaseDatabaseId());
}

void StoreWindow::onDownloadError(const QString &error)
{
    setBusy(false);
    m_progressBar->setVisible(false);
    m_statusLabel->setText("Download error: " + error);
}

bool StoreWindow::isInstalled(const QString &themeId) const
{
    QDir dir(m_config->themesUserDir() + "/" + themeId);
    if (!dir.exists())
        return false;

    const QStringList themes = dir.entryList({QStringLiteral("*.theme")}, QDir::Files);
    return !themes.isEmpty();
}

void StoreWindow::addCard(const QString &themeId, const ThemeData &theme)
{
    auto *card = new ThemeCard(theme, m_gridHost);
    card->setThemeId(themeId);
    card->setInstalled(isInstalled(themeId));

    connect(card, &ThemeCard::installRequested,
            this, [this](const QString &id, const ThemeData &t) {
        onInstallClicked(id, t);
    });
    connect(card, &ThemeCard::donateRequested, this, [](const QUrl &url) {
        QDesktopServices::openUrl(url);
    });
    connect(card, &ThemeCard::likeRequested, this, [this](const QString &id, const ThemeData &t) {
        onLikeClicked(id, t);
    });

    int index = m_cards.size();
    int row = index / m_columns;
    int col = index % m_columns;
    m_grid->addWidget(card, row, col);
    // Let every column share the available width equally so a row of cards
    // fills the full scroll area width instead of leaving unused space on the right.
    m_grid->setColumnStretch(col, 1);
    m_cards.append(card);
}

void StoreWindow::clearGrid()
{
    for (ThemeCard *card : m_cards) {
        m_grid->removeWidget(card);
        card->deleteLater();
    }
    m_cards.clear();
    m_layoutHint->setVisible(false);
    if (m_loadingMoreLabel)
        m_loadingMoreLabel->setVisible(false);
}

void StoreWindow::setBusy(bool busy)
{
    m_refreshBtn->setEnabled(!busy);
    if (busy) {
        m_progressBar->setRange(0, 0);
        m_progressBar->setVisible(true);
    } else if (!m_loadingMore) {
        m_progressBar->setVisible(false);
    }
}

void StoreWindow::setLoadingMore(bool loading)
{
    m_loadingMore = loading;

    if (!m_loadingMoreLabel)
        return;

    m_loadingMoreLabel->setVisible(true);
    m_loadingMoreLabel->setText(loading ? QStringLiteral("Loading more...")
                                        : QStringLiteral("End of list"));
}

bool StoreWindow::shouldLoadMore() const
{
    if (!m_hasMorePages || m_loadingMore)
        return false;

    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    if (bar->maximum() <= 0)
        return true; // content not scrollable yet -> load more eagerly

    // Load the next page when the user scrolled near the bottom.
    return bar->value() >= bar->maximum() - 300;
}

void StoreWindow::fetchNextPage()
{
    if (!m_hasMorePages || m_loadingMore)
        return;

    setLoadingMore(true);
    m_netMgr->fetchThemeList(m_config->firebaseApiKey(),
                             m_config->firebaseProjectId(),
                             m_config->firebaseDatabaseId(),
                             m_pageSize, m_nextPageToken);
}

void StoreWindow::onScrollRangeChanged(int min, int max)
{
    Q_UNUSED(min);
    Q_UNUSED(max);
    if (shouldLoadMore())
        fetchNextPage();
}