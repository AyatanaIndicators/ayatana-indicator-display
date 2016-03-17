/*
 * Copyright 2016 Canonical Ltd.
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
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#pragma once

#define QT_NO_KEYWORDS

#include <tests/utils/dbus-types.h>
#include <tests/utils/qdbus-helpers.h>
#include <tests/utils/glib-fixture.h>
#include <tests/utils/gtest-qt-print-helpers.h>

#include <gtest/gtest.h>

#include <QSignalSpy>

class QtFixture: public GlibFixture
{
    using super = GlibFixture;

public:

    QtFixture()
    {
        DBusTypes::registerMetaTypes();
    }

    ~QtFixture() =default;

protected:

    void wait_for_signals(QSignalSpy& signalSpy, int signalsExpected)
    {
        while (signalSpy.size() < signalsExpected)
        {
            ASSERT_TRUE(signalSpy.wait());
        }

        ASSERT_EQ(signalsExpected, signalSpy.size());
    }
};

