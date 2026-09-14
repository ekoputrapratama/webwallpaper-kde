#pragma once

#include <QJsonObject>
#include <QString>

#include "themedata.h"

// Parses a Firestore `wallpapers` document's `fields` object into ThemeData,
// following the schema published by the Theme Submitter web app
// (stringValue / integerValue / arrayValue-wrapped scalars).
ThemeData parseTheme(const QJsonObject &fields);

// Turns a display name into a filesystem-safe install directory name, mirroring
// the web app's slug rules (lowercase; runs of non [a-z0-9] become '-', then
// leading/trailing '-' are trimmed). Empty input falls back to "theme".
QString slugify(const QString &name);