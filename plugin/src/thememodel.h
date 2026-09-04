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

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Theme {
        QString name;
        QString description;
        QString path;
        QString preview;
        QString entry;
    };

    void scanThemes();

    QList<Theme> m_themes;
};