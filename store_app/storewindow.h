#pragma once

#include <QDialog>
#include <QGridLayout>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QLineEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QHash>
#include <QSet>

#include "themedata.h"

class NetworkManager;
class ConfigManager;
class ThemeCard;
class QScrollArea;
class QTimer;
class QHBoxLayout;

class StoreWindow : public QDialog
{
    Q_OBJECT

public:
    explicit StoreWindow(NetworkManager *netMgr, ConfigManager *config, QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onFetchThemes();
    void onSearchChanged();
    void onTagToggled(bool checked);
    void onClearFilters();
    void onThemePageReceived(const QJsonDocument &doc, const QString &nextPageToken);
    void onFetchError(const QString &error);
    void onScrollRangeChanged(int min, int max);
    void onImageFetched(const QUrl &url, const QByteArray &data);
    void onImageFailed(const QUrl &url);
    void onInstallClicked(const QString &themeId, const ThemeData &theme);
    void onRemoveClicked(const QString &themeId, const ThemeData &theme);
    void onLikeClicked(const QString &themeId, const ThemeData &theme);
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(const QString &themeDir);
    void onDownloadError(const QString &error);

private:
    struct ThemeEntry {
        QString themeId;
        ThemeData theme;
    };

    bool isInstalled(const QString &themeId) const;
    void clearGrid();
    ThemeCard *createCardForIndex(int index, const ThemeEntry &entry);
    void refreshVisibleCards();
    void setBusy(bool busy);
    void setLoadingMore(bool loading);
    void fetchNextPage();
    bool shouldLoadMore() const;

    bool filterActive() const;
    bool matchesFilter(const ThemeEntry &entry) const;
    void collectAvailableTags();
    void rebuildTagButtons();
    void applyFilter();

    NetworkManager *m_netMgr;
    ConfigManager *m_config;

    QScrollArea *m_scrollArea;
    QWidget *m_gridHost;
    QGridLayout *m_grid;
    QPushButton *m_refreshBtn;
    QPushButton *m_wallpaperSettingsBtn;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QLabel *m_layoutHint;
    QLineEdit *m_searchEdit;
    QScrollArea *m_tagScroll = nullptr;
    QWidget *m_tagStripHost;
    QHBoxLayout *m_tagStrip;
    QPushButton *m_clearFiltersBtn;
    QTimer *m_searchDebounce = nullptr;
    QVector<QPushButton *> m_tagButtons;

    QVector<ThemeCard *> m_cards;
    QHash<QUrl, QString> m_themeIdByThumbUrl;
    QHash<QUrl, QByteArray> m_thumbnailCache;
    QVector<ThemeEntry> m_themeEntries;
    QHash<int, ThemeCard *> m_cardByIndex;
    QHash<QString, QString> m_themeDocPath;
    QSet<QString> m_selectedTags;
    QVector<int> m_filteredIndices;
    QString m_selectedThemeId;
    QString m_nextPageToken;
    bool m_hasMorePages = false;
    bool m_loadingMore = false;
    bool m_refreshingWindow = false;
    bool m_filtersDirty = false;
    QLabel *m_loadingMoreLabel = nullptr;
    int m_page = 0;
    int m_columns = 3;
    int m_pageSize = 10;
    int m_cardHeight = 380;
    int m_cardSpacing = 16;
};