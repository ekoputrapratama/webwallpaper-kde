#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include "themedata.h"

class QVBoxLayout;
class QBuffer;
class QMovie;

class ThemeCard : public QFrame
{
    Q_OBJECT

public:
    explicit ThemeCard(const ThemeData &theme, QWidget *parent = nullptr);

    ThemeData themeData() const { return m_theme; }
    QString themeId() const { return m_themeId; }
    void setThemeId(const QString &id) { m_themeId = id; }
    void setThumbnail(const QByteArray &imageData);
    void setInstalled(bool installed);
    void setLikes(qint64 likes);

signals:
    void installRequested(const QString &themeId, const ThemeData &theme);
    void donateRequested(const QUrl &url);
    void likeRequested(const QString &themeId, const ThemeData &theme);

private:
    void clearMovie();

    QLabel *m_thumbnailLabel;
    QPushButton *m_installBtn;
    QPushButton *m_likeBtn;
    QLabel *m_likeLabel;
    QByteArray m_thumbnailData;
    QBuffer *m_movieBuffer = nullptr;
    QMovie *m_movie = nullptr;
    bool m_installed = false;

    ThemeData m_theme;
    QString m_themeId;
};