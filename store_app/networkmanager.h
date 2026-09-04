#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QUrl>

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);

    void fetchThemeList(const QString &apiKey, const QString &projectId, const QString &databaseId,
                         int pageSize = 10, const QString &pageToken = QString());
    void downloadTheme(const QString &url, const QString &destDir, const QString &themeName);
    void fetchImage(const QUrl &url);
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

    QNetworkAccessManager m_nam;
    bool m_busy = false;
};
