#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

// Mirrors the Firestore `wallpapers` collection document schema
// (the catalog the web-kit-wallpaper web app publishes from).
struct ThemeData {
    QString name;
    QString description;
    QString author;
    QStringList tags;
    QUrl thumbnailUrl;   // thumbnail_url
    QUrl downloadUrl;    // wallpaper_url (zip)
    QUrl donateUrl;      // donation_url
    QString donationLabel;
    qint64 downloads = 0;
    qint64 likes = 0;
    QString uid;

    bool isValid() const { return !name.isEmpty() && !downloadUrl.isEmpty(); }
};