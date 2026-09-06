#pragma once

#include <QAbstractListModel>
#include <QDir>
#include <QHash>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>

class ThemeModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString defaultThemePath READ defaultThemePath NOTIFY defaultThemePathChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        DescriptionRole,
        PathRole,
        PreviewRole,
        EntryRole,
    };
    Q_ENUM(Roles)

    explicit ThemeModel(QObject *parent = nullptr);

    QString defaultThemePath() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void defaultThemePathChanged();

private:
    struct Theme {
        QString name;
        QString description;
        QString path;
        QString preview;
        QString entry;
    };

    void scanThemes();
    void scanDir(const QString &dirPath);

    QList<Theme> m_themes;
    QString m_defaultThemePath;
};