/*
 * Copyright (C) 2013-2016 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Pete Woods <pete.woods@canonical.com>
 */

#pragma once

#include <QDBusMetaType>
#include <QtCore>
#include <QString>
#include <QVariantMap>

typedef QMap<QString, QVariantMap> QVariantDictMap;
Q_DECLARE_METATYPE(QVariantDictMap)

typedef QMap<QString, QString> QStringMap;
Q_DECLARE_METATYPE(QStringMap)

namespace DBusTypes
{
    inline void registerMetaTypes()
    {
        qRegisterMetaType<QVariantDictMap>("QVariantDictMap");
        qRegisterMetaType<QStringMap>("QStringMap");

        qDBusRegisterMetaType<QVariantDictMap>();
        qDBusRegisterMetaType<QStringMap>();
    }
}
