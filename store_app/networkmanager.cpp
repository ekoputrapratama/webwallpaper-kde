#include "networkmanager.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QProcess>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
{
}

bool NetworkManager::isBusy() const
{
    return m_busy;
}

void NetworkManager::fetchThemeList(const QString &apiKey, const QString &projectId, const QString &databaseId,
                                     int pageSize, const QString &pageToken)
{
    if (m_busy) return;
    m_busy = true;

    QString db = databaseId.isEmpty() ? QStringLiteral("(default)") : databaseId;

    QString url = QString("https://firestore.googleapis.com/v1/projects/%1/databases/%2/documents/wallpapers?key=%3&pageSize=%4")
        .arg(projectId, db, apiKey).arg(pageSize);
    if (!pageToken.isEmpty())
        url += QStringLiteral("&pageToken=%1").arg(pageToken);

    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("Accept", "application/json");
    QNetworkReply *reply = m_nam.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_busy = false;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit fetchError(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString nextPageToken = doc[QStringLiteral("nextPageToken")].toString();
        emit themePageFetched(doc, nextPageToken);
    });
}

void NetworkManager::downloadTheme(const QString &url, const QString &destDir, const QString &themeName)
{
    if (m_busy) return;
    m_busy = true;

    QDir().mkpath(destDir);

    QNetworkRequest req{QUrl(url)};
    QNetworkReply *reply = m_nam.get(req);

    connect(reply, &QNetworkReply::downloadProgress,
            this, &NetworkManager::downloadProgress);

    connect(reply, &QNetworkReply::finished, this, [this, reply, destDir, themeName]() {
        m_busy = false;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit downloadError(reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        QString zipPath = destDir + "/" + themeName + ".zip";
        QFile file(zipPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
        }

        if (!extractZip(zipPath, destDir)) {
            emit downloadError(QStringLiteral("Failed to extract %1").arg(zipPath));
            return;
        }
        QFile::remove(zipPath);

        emit downloadFinished(destDir);
    });
}

void NetworkManager::fetchImage(const QUrl &url)
{
    QNetworkRequest req{url};
    req.setRawHeader("Accept", "image/*");
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit imageFailed(url);
            return;
        }
        emit imageFetched(url, reply->readAll());
    });
}

void NetworkManager::incrementDownload(const QString &documentPath, const QString &apiKey, const QString &projectId, const QString &databaseId)
{
    incrementField(documentPath, QStringLiteral("downloads"), apiKey, projectId, databaseId);
}

void NetworkManager::incrementLikes(const QString &documentPath, const QString &apiKey, const QString &projectId, const QString &databaseId)
{
    incrementField(documentPath, QStringLiteral("likes"), apiKey, projectId, databaseId);
}

void NetworkManager::incrementField(const QString &documentPath, const QString &field, const QString &apiKey, const QString &projectId, const QString &databaseId)
{
    QString db = databaseId.isEmpty() ? QStringLiteral("(default)") : databaseId;

    QString url = QString("https://firestore.googleapis.com/v1/projects/%1/databases/%2/documents:commit?key=%3")
        .arg(projectId, db, apiKey);

    QJsonObject increment;
    increment[QStringLiteral("integerValue")] = 1;
    QJsonObject fieldTransform;
    fieldTransform[QStringLiteral("fieldPath")] = field;
    fieldTransform[QStringLiteral("increment")] = increment;

    QJsonObject update;
    update[QStringLiteral("name")] = documentPath;
    QJsonObject updateMask;
    updateMask[QStringLiteral("fieldPaths")] = QJsonArray{ field };

    QJsonObject write;
    write[QStringLiteral("update")] = update;
    write[QStringLiteral("updateMask")] = updateMask;
    write[QStringLiteral("updateTransforms")] = QJsonArray{ fieldTransform };

    QJsonObject body;
    body[QStringLiteral("writes")] = QJsonArray{ write };

    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("Accept", "application/json");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, documentPath]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit downloadCountIncremented(documentPath);
    });
}

bool NetworkManager::extractZip(const QString &zipPath, const QString &destDir) const
{
    QDir().mkpath(destDir);

    QProcess unzip;
    unzip.setProcessChannelMode(QProcess::MergedChannels);
    unzip.start(QStringLiteral("unzip"), { QStringLiteral("-o"), zipPath, QStringLiteral("-d"), destDir });
    if (!unzip.waitForStarted(5000))
        return false;
    if (!unzip.waitForFinished(30000))
        return false;

    return unzip.exitStatus() == QProcess::NormalExit && unzip.exitCode() == 0;
}