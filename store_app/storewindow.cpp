#include "storewindow.h"
#include "networkmanager.h"
#include "configmanager.h"
#include "themecard.h"
#include "firestore.h"

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
#include <QMessageBox>
#include <QTimer>
#include <QMap>
#include <QWheelEvent>

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

    // Search bar
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search themes by name, author or tag..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(
        "QLineEdit { background: #26262c; color: #f2f2f4; border: 1px solid #33333a;"
        " border-radius: 6px; padding: 8px 12px; font-size: 13px; }"
        "QLineEdit:focus { border-color: #3d7eff; }");
    outer->addWidget(m_searchEdit);

    // Tag filter strip
    auto *tagRow = new QHBoxLayout();
    auto *tagLabel = new QLabel(QStringLiteral("Tags:"), this);
    tagLabel->setStyleSheet("color: #9a9aa2; font-size: 12px;");
    tagRow->addWidget(tagLabel, 0, Qt::AlignVCenter);

    auto *tagScroll = new QScrollArea(this);
    tagScroll->setWidgetResizable(true);
    tagScroll->setFrameShape(QFrame::NoFrame);
    tagScroll->setFixedHeight(38);
    tagScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tagScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tagScroll->setStyleSheet(
        "QScrollBar:horizontal { background: #1c1c21; height: 6px; margin: 2px 0; }"
        "QScrollBar::handle:horizontal { background: #44444c; border-radius: 3px; min-width: 40px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }");

    m_tagStripHost = new QWidget();
    m_tagStrip = new QHBoxLayout(m_tagStripHost);
    m_tagStrip->setContentsMargins(0, 0, 0, 0);
    m_tagStrip->setSpacing(6);
    tagScroll->setWidget(m_tagStripHost);
    tagRow->addWidget(tagScroll, 1);
    m_tagScroll = tagScroll;
    m_tagScroll->viewport()->installEventFilter(this);
    m_tagStripHost->installEventFilter(this);

    m_clearFiltersBtn = new QPushButton(QStringLiteral("Clear"), this);
    m_clearFiltersBtn->setVisible(false);
    m_clearFiltersBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #e05a7a; border: none;"
        " font-size: 12px; font-weight: 600; padding: 4px 8px; }");
    tagRow->addWidget(m_clearFiltersBtn, 0, Qt::AlignVCenter);

    outer->addLayout(tagRow);

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
    connect(m_searchEdit, &QLineEdit::textChanged, this, &StoreWindow::onSearchChanged);
    connect(m_clearFiltersBtn, &QPushButton::clicked, this, &StoreWindow::onClearFilters);

    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(250);
    connect(m_searchDebounce, &QTimer::timeout, this, &StoreWindow::applyFilter);
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
    connect(m_netMgr, &NetworkManager::imageFailed,
            this, &StoreWindow::onImageFailed);
    connect(m_netMgr, &NetworkManager::downloadProgress,
            this, &StoreWindow::onDownloadProgress);
    connect(m_netMgr, &NetworkManager::downloadFinished,
            this, &StoreWindow::onDownloadFinished);
    connect(m_netMgr, &NetworkManager::downloadError,
            this, &StoreWindow::onDownloadError);

    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::rangeChanged,
            this, &StoreWindow::onScrollRangeChanged);
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &StoreWindow::refreshVisibleCards);

    onFetchThemes();
}

void StoreWindow::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    refreshVisibleCards();
}

bool StoreWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (m_tagScroll
        && (watched == m_tagScroll->viewport() || watched == m_tagStripHost)
        && event->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent *>(event);
        // Route vertical wheel motion to the horizontal tag strip so scrolling
        // over the tags doesn't scroll the theme grid underneath.
        QScrollBar *hbar = m_tagScroll->horizontalScrollBar();
        if (hbar && hbar->minimum() < hbar->maximum()) {
            const int delta = we->angleDelta().y() != 0
                ? we->angleDelta().y()
                : we->angleDelta().x();
            hbar->setValue(hbar->value() - delta);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
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
    applyFilter();
    m_netMgr->fetchThemeList(apiKey, projectId, databaseId, m_pageSize);
}

void StoreWindow::onThemePageReceived(const QJsonDocument &doc, const QString &nextPageToken)
{
    setBusy(false);

    const QJsonArray documents = doc["documents"].toArray();
    const int prevCount = m_themeEntries.size();

    for (const auto &val : documents) {
        QJsonObject obj = val.toObject();
        QJsonObject fields = obj["fields"].toObject();

        ThemeData theme = ::parseTheme(fields);
        if (!theme.isValid())
            continue;

        QString themeId = slugify(theme.name);
        if (themeId.isEmpty())
            themeId = theme.name;

        m_themeDocPath.insert(themeId, obj["name"].toString());

        if (theme.thumbnailUrl.isValid())
            m_themeIdByThumbUrl.insert(theme.thumbnailUrl, themeId);

        m_themeEntries.append({themeId, theme});
    }

    m_nextPageToken = nextPageToken;
    m_hasMorePages = !nextPageToken.isEmpty();
    m_page++;

    collectAvailableTags();
    applyFilter();

    // Refresh the bottom indicator: visible "Loading more..." while there's a
    // next page, "End of list" when exhausted. It stays hidden if content
    // doesn't fill the viewport yet (auto-load happens below).
    m_loadingMore = false;
    if (m_loadingMoreLabel) {
        bool exhausted = !m_hasMorePages && !m_themeEntries.isEmpty();
        m_loadingMoreLabel->setVisible(exhausted);
        if (exhausted)
            m_loadingMoreLabel->setText(QStringLiteral("End of list"));
    }

    refreshVisibleCards();

    if (prevCount == 0 && m_themeEntries.isEmpty()) {
        m_statusLabel->setText(QString("No themes published yet (%1 docs scanned).").arg(documents.size()));
        m_layoutHint->setVisible(true);
    } else if (!m_filteredIndices.isEmpty()) {
        m_layoutHint->setVisible(false);
        if (filterActive())
            m_statusLabel->setText(QStringLiteral("%1 matching themes.").arg(m_filteredIndices.size()));
        else
            m_statusLabel->setText(QStringLiteral("%1 themes loaded.").arg(m_themeEntries.size()));
    }

    // While a filter is active, keep pulling pages so the result set is
    // complete instead of only covering what the user has scrolled through.
    if (m_hasMorePages && (filterActive() || shouldLoadMore()))
        fetchNextPage();
}

void StoreWindow::onFetchError(const QString &error)
{
    setBusy(false);
    m_statusLabel->setText("Error: " + error);
}

void StoreWindow::onImageFetched(const QUrl &url, const QByteArray &data)
{
    m_thumbnailCache.insert(url, data); // reuse instantly when the card is re-materialized

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

void StoreWindow::onImageFailed(const QUrl &url)
{
    QString id = m_themeIdByThumbUrl.value(url);
    if (id.isEmpty())
        return;

    for (ThemeCard *card : m_cards) {
        if (card->themeId() == id) {
            card->setThumbnailFailed();
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

void StoreWindow::onRemoveClicked(const QString &themeId, const ThemeData &theme)
{
    const QString themeDir = m_config->themesUserDir() + "/" + themeId;
    const QString displayName = theme.name.isEmpty() ? themeId : theme.name;

    auto answer = QMessageBox::question(
        this, QStringLiteral("Remove theme"),
        QStringLiteral("Uninstall \"%1\"? This deletes the theme files from %2.")
            .arg(displayName, themeDir),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QDir dir(themeDir);
    if (dir.exists() && !dir.removeRecursively()) {
        m_statusLabel->setText("Failed to remove " + displayName);
        return;
    }

    for (ThemeCard *card : m_cards) {
        if (card->themeId() == themeId) {
            card->setInstalled(false);
            break;
        }
    }
    m_statusLabel->setText(displayName + " removed.");
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

ThemeCard *StoreWindow::createCardForIndex(int index, const ThemeEntry &entry)
{
    auto *card = new ThemeCard(entry.theme, m_gridHost);
    card->setThemeId(entry.themeId);
    card->setInstalled(isInstalled(entry.themeId));

    connect(card, &ThemeCard::installRequested,
            this, [this](const QString &id, const ThemeData &t) {
        onInstallClicked(id, t);
    });
    connect(card, &ThemeCard::donateRequested, this, [](const QUrl &url) {
        QDesktopServices::openUrl(url);
    });
    connect(card, &ThemeCard::removeRequested, this, [this](const QString &id, const ThemeData &t) {
        onRemoveClicked(id, t);
    });
    connect(card, &ThemeCard::likeRequested, this, [this](const QString &id, const ThemeData &t) {
        onLikeClicked(id, t);
    });

    int row = index / m_columns;
    int col = index % m_columns;
    m_grid->addWidget(card, row, col);
    // Let every column share the available width equally so a row of cards
    // fills the full scroll area width instead of leaving unused space on the right.
    m_grid->setColumnStretch(col, 1);
    m_cards.append(card);
    m_cardByIndex.insert(index, card);

    // Thumbnails are cheap to serve from the in-memory cache; otherwise the
    // NetworkManager pulls from its on-disk cache or the network (one at a time).
    const QUrl thumbUrl = entry.theme.thumbnailUrl;
    if (thumbUrl.isValid()) {
        const QByteArray cached = m_thumbnailCache.value(thumbUrl);
        if (!cached.isEmpty()) {
            card->setThumbnail(cached);
        } else {
            m_netMgr->fetchImage(thumbUrl);
        }
    }

    return card;
}

void StoreWindow::refreshVisibleCards()
{
    if (m_refreshingWindow)
        return;
    m_refreshingWindow = true;

    const int total = m_filteredIndices.size();
    if (total == 0) {
        m_refreshingWindow = false;
        return;
    }

    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    const int rowHeight = m_cardHeight + m_cardSpacing;
    const int viewH = m_scrollArea->viewport()->height();
    const int scroll = bar->value();

    // Materialize a couple of rows above and below the visible viewport so
    // scrolling ahead stays smooth while thumbnails/movies load.
    const int bufferRows = 2;
    const int firstRow = qMax(0, scroll / rowHeight - bufferRows);
    const int lastRow = qMin((scroll + viewH) / rowHeight + bufferRows,
                             (total - 1) / m_columns);
    const int firstIndex = firstRow * m_columns;
    const int lastIndex = lastRow * m_columns + (m_columns - 1);

    // Reap cards that scrolled out of range.
    for (auto it = m_cardByIndex.begin(); it != m_cardByIndex.end();) {
        const int idx = it.key();
        if (idx < firstIndex || idx > lastIndex) {
            ThemeCard *card = it.value();
            m_grid->removeWidget(card);
            card->deleteLater();
            m_cards.removeOne(card);
            it = m_cardByIndex.erase(it);
        } else {
            ++it;
        }
    }

    // Materialize any visible card that isn't created yet.
    for (int pos = firstIndex; pos <= lastIndex; ++pos) {
        if (pos < total && !m_cardByIndex.contains(pos))
            createCardForIndex(pos, m_themeEntries[m_filteredIndices[pos]]);
    }

    m_refreshingWindow = false;
}

void StoreWindow::clearGrid()
{
    for (ThemeCard *card : m_cards) {
        m_grid->removeWidget(card);
        card->deleteLater();
    }
    m_cards.clear();
    m_cardByIndex.clear();
    m_themeEntries.clear();
    m_themeIdByThumbUrl.clear();
    m_filteredIndices.clear();
    m_selectedTags.clear();
    m_clearFiltersBtn->setVisible(false);

    while (m_tagStrip->count() > 0) {
        QLayoutItem *item = m_tagStrip->takeAt(0);
        delete item->widget();
        delete item;
    }
    m_tagButtons.clear();

    m_layoutHint->setVisible(false);
    if (m_loadingMoreLabel)
        m_loadingMoreLabel->setVisible(false);
}

bool StoreWindow::filterActive() const
{
    return !m_searchEdit->text().trimmed().isEmpty() || !m_selectedTags.isEmpty();
}

bool StoreWindow::matchesFilter(const ThemeEntry &entry) const
{
    const QString needle = m_searchEdit->text().trimmed().toLower();
    if (!needle.isEmpty()) {
        const ThemeData &t = entry.theme;
        QString haystack = t.name.toLower();
        haystack += QLatin1Char(' ') + t.author.toLower();
        haystack += QLatin1Char(' ') + t.tags.join(QLatin1Char(' ')).toLower();
        if (!haystack.contains(needle))
            return false;
    }

    if (!m_selectedTags.isEmpty()) {
        for (const QString &tag : std::as_const(m_selectedTags)) {
            if (!entry.theme.tags.contains(tag))
                return false;
        }
    }

    return true;
}

void StoreWindow::collectAvailableTags()
{
    // Accumulate into a sorted map (QMap) to sidestep Qt 6 COW container
    // asserts that fire inside QSet::values()/QList::reserve on this build.
    QMap<QString, bool> known;
    for (const ThemeEntry &entry : m_themeEntries) {
        for (const QString &tag : entry.theme.tags)
            known.insert(tag, true);
    }

    const int want = known.size();
    const bool changed = (m_tagStrip->count() - 1) != want; // last item is a stretch
    if (!changed)
        return;

    while (m_tagStrip->count() > 0) {
        QLayoutItem *item = m_tagStrip->takeAt(0);
        delete item->widget();
        delete item;
    }

    m_tagButtons.clear();
    for (auto it = known.constBegin(); it != known.constEnd(); ++it) {
        const QString tag = it.key();
        auto *btn = new QPushButton(tag, m_tagStripHost);
        btn->setCheckable(true);
        btn->setChecked(m_selectedTags.contains(tag));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(26);
        btn->setStyleSheet(
            "QPushButton { background: transparent; color: #c0c0c6; border: 1px solid #3a3a42;"
            " border-radius: 13px; padding: 0 12px; font-size: 12px; }"
            "QPushButton:hover { border-color: #7aa2f7; color: #f2f2f4; }"
            "QPushButton:checked { background: #22304a; color: #7aa2f7; border-color: #3d7eff; }");
        connect(btn, &QPushButton::toggled, this, &StoreWindow::onTagToggled);
        m_tagStrip->addWidget(btn);
        m_tagButtons.append(btn);
    }
    m_tagStrip->addStretch();

    // Let the strip host report its full content width so the scroll area
    // shows a horizontal scrollbar when the tags overflow the viewport.
    m_tagStripHost->setMinimumWidth(m_tagStrip->sizeHint().width());
}

void StoreWindow::rebuildTagButtons()
{
    // Drop any selection that no longer exists in the catalog, then re-render.
    for (QSet<QString>::iterator it = m_selectedTags.begin(); it != m_selectedTags.end();) {
        bool exists = false;
        for (const ThemeEntry &entry : m_themeEntries) {
            if (entry.theme.tags.contains(*it)) {
                exists = true;
                break;
            }
        }
        if (!exists)
            it = m_selectedTags.erase(it);
        else
            ++it;
    }
    while (m_tagStrip->count() > 0) {
        QLayoutItem *item = m_tagStrip->takeAt(0);
        delete item->widget();
        delete item;
    }
    m_tagButtons.clear();
    m_tagStripHost->setMinimumWidth(0);

    collectAvailableTags();
    m_clearFiltersBtn->setVisible(filterActive());
}

void StoreWindow::applyFilter()
{
    m_filteredIndices.clear();
    for (int i = 0; i < m_themeEntries.size(); ++i) {
        if (matchesFilter(m_themeEntries[i]))
            m_filteredIndices.append(i);
    }

    // Positions shifted -> drop every materialized card and re-window.
    for (ThemeCard *card : m_cards) {
        m_grid->removeWidget(card);
        card->deleteLater();
    }
    m_cards.clear();
    m_cardByIndex.clear();

    m_clearFiltersBtn->setVisible(filterActive());
    m_layoutHint->setVisible(m_filteredIndices.isEmpty());
    if (m_filteredIndices.isEmpty())
        m_statusLabel->setText(QStringLiteral("No themes match."));
    else
        m_statusLabel->setText(QStringLiteral("%1 of %2 themes.").arg(m_filteredIndices.size()).arg(m_themeEntries.size()));

    if (m_scrollArea->verticalScrollBar()->maximum() > 0)
        m_scrollArea->verticalScrollBar()->setValue(0);

    refreshVisibleCards();
}

void StoreWindow::onSearchChanged()
{
    m_searchDebounce->start();
}

void StoreWindow::onTagToggled(bool checked)
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;
    if (checked)
        m_selectedTags.insert(btn->text());
    else
        m_selectedTags.remove(btn->text());
    applyFilter();

    // While filtering, pull the remaining pages so results are complete.
    if (m_hasMorePages && !m_loadingMore)
        fetchNextPage();
}

void StoreWindow::onClearFilters()
{
    m_selectedTags.clear();
    m_searchEdit->clear();
    m_clearFiltersBtn->setVisible(false);
    for (QPushButton *btn : m_tagButtons)
        btn->setChecked(false);
    applyFilter();
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
    refreshVisibleCards();
    if (shouldLoadMore())
        fetchNextPage();
}