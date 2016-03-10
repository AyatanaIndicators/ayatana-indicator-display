
#pragma once

#include <QDBusObjectPath>
#include <QString>
#include <QStringList>
#include <QVariant>

inline QString qVariantToString(const QVariant& variant) {
    QString output;
    QDebug dbg(&output);
    dbg << variant;
    return output;
}

inline void PrintTo(const QVariant& variant, std::ostream* os) {
    QString output;
    QDebug dbg(&output);
    dbg << variant;

    *os << "QVariant(" << output.toStdString() << ")";
}

inline void PrintTo(const QString& s, std::ostream* os) {
    *os << "\"" << s.toStdString() << "\"";
}

inline void PrintTo(const QStringList& list, std::ostream* os) {
    QString output;
    QDebug dbg(&output);
    dbg << list;

    *os << "QStringList(" << output.toStdString() << ")";
}

inline void PrintTo(const QList<QDBusObjectPath>& list, std::ostream* os) {
    QString output;
    for (const auto& path: list)
    {
        output.append("\"" + path.path() + "\",");
    }

    *os << "QList<QDBusObjectPath>(" << output.toStdString() << ")";
}

