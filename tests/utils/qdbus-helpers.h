#pragma once

#define QT_NO_KEYWORDS
#include <QDBusArgument>
#include <QVariant>

bool qDBusArgumentToMap(QVariant const& variant, QVariantMap& map)
{
    if (variant.canConvert<QDBusArgument>())
    {
        QDBusArgument value(variant.value<QDBusArgument>());
        if (value.currentType() == QDBusArgument::MapType)
        {
            value >> map;
            return true;
        }
    }

    return false;
}

