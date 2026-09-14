#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QUrl>
#include <QQueue>
#include <QSet>

class QTimer;

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);

    void fetchThemeList(const QString &apiKey, const QString &projectId, const QString &databaseId,
                         int pageSize = 10, const QString &pageToken = QString());
    void downloadTheme(const QString &url, const QString &destDir, const QString &themeName);
    void fetchImage(const QUrl &url);

    static QString thumbnailCacheDir();
    static QString thumbnailCachePath(const QUrl &url);
    static void saveThumbnailCache(const QUrl &url, const QByteArray &data);
    void incrementDownload(const QString &documentPath, const QString &apiKey, const QString &projectId, const QString &databaseId);
    void incrementField(const QString &documentPath, const QString &field, const QString &apiKey, const QString &projectId, const QString &databaseId);
    void incrementLikes(const QString &documentPath, const QString &apiKey, const QString &projectId, const QString &databaseId);

    bool isBusy() const;

signals:
    void themePageFetched(const QJsonDocument &doc, const QString &nextPageToken);
    void fetchError(const QString &error);
    void imageFetched(const QUrl &url, const QByteArray &data);
    void imageFailed(const QUrl &url);
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString &themeDir);
    void downloadError(const QString &error);
    void downloadCountIncremented(const QString &documentPath);

private:
    bool extractZip(const QString &zipPath, const QString &destDir) const;
    void processNextThumbnail();

    QNetworkAccessManager m_nam;
    bool m_busy = false;

    // Thumbnails are downloaded one at a time: pulling many large-to-medium
    // images concurrently on limited connections starves every transfer, which
    // left GIF previews stuck on "Loading..." forever.
    QQueue<QUrl> m_thumbnailQueue;
    QSet<QUrl> m_pendingImageUrls;
    QNetworkReply *m_activeThumbnailReply = nullptr;
    QUrl m_activeThumbnailUrl;
    QTimer *m_thumbnailTimeout = nullptr;
};
