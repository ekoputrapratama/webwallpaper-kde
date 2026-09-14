#include "firestore.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>

ThemeData parseTheme(const QJsonObject &fields)
{
    ThemeData theme;
    const auto str = [&fields](const char *key) -> QString {
        return fields[key].toObject()[QStringLiteral("stringValue")].toString();
    };
    theme.name = str("name");
    theme.description = str("description");
    theme.author = str("author");
    theme.thumbnailUrl = QUrl(str("thumbnail_url"));
    theme.downloadUrl = QUrl(str("wallpaper_url"));
    theme.donateUrl = QUrl(str("donation_url"));
    theme.donationLabel = str("donation_label");
    theme.uid = str("uid");
    theme.downloads = fields[QStringLiteral("downloads")].toObject()[QStringLiteral("integerValue")].toString().toLongLong();
    theme.likes = fields[QStringLiteral("likes")].toObject()[QStringLiteral("integerValue")].toString().toLongLong();

    const QJsonArray tags = fields[QStringLiteral("tags")]
                                .toObject()[QStringLiteral("arrayValue")]
                                .toObject()[QStringLiteral("values")]
                                .toArray();
    for (const QJsonValue &tag : tags)
        theme.tags << tag.toObject()[QStringLiteral("stringValue")].toString();

    return theme;
}

QString slugify(const QString &name)
{
    QString slug = name.toLower();
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    slug.remove(QRegularExpression(QStringLiteral("^-|-$")));
    return slug.isEmpty() ? QStringLiteral("theme") : slug;
}