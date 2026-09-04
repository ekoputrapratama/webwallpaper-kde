#pragma once

#include <QDialog>
#include <QGridLayout>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QHash>

#include "themedata.h"

class NetworkManager;
class ConfigManager;
class ThemeCard;
class QScrollArea;

class StoreWindow : public QDialog
{
    Q_OBJECT

public:
    explicit StoreWindow(NetworkManager *netMgr, ConfigManager *config, QWidget *parent = nullptr);

private slots:
    void onFetchThemes();
    void onThemePageReceived(const QJsonDocument &doc, const QString &nextPageToken);
    void onFetchError(const QString &error);
    void onScrollRangeChanged(int min, int max);
    void onImageFetched(const QUrl &url, const QByteArray &data);
    void onInstallClicked(const QString &themeId, const ThemeData &theme);
    void onLikeClicked(const QString &themeId, const ThemeData &theme);
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(const QString &themeDir);
    void onDownloadError(const QString &error);

private:
    ThemeData parseTheme(const QJsonObject &fields) const;
    bool isInstalled(const QString &themeId) const;
    void clearGrid();
    void addCard(const QString &themeId, const ThemeData &theme);
    void setBusy(bool busy);
    void setLoadingMore(bool loading);
    void fetchNextPage();
    bool shouldLoadMore() const;

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

    QVector<ThemeCard *> m_cards;
    QHash<QUrl, QString> m_themeIdByThumbUrl;
    QHash<QString, QString> m_themeDocPath;
    QString m_selectedThemeId;
    QString m_nextPageToken;
    bool m_hasMorePages = false;
    bool m_loadingMore = false;
    QLabel *m_loadingMoreLabel = nullptr;
    int m_page = 0;
    int m_columns = 3;
    int m_pageSize = 10;
};