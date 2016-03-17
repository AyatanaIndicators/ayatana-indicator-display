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

namespace DBusNames
{
    namespace Notify
    {
        static constexpr char const * NAME = "org.freedesktop.Notifications";
        static constexpr char const * PATH = "/org/freedesktop/Notifications";
        static constexpr char const * INTERFACE = "org.freedesktop.Notifications";

        namespace ActionInvoked
        {
            static constexpr char const * NAME = "ActionInvoked";
        }

        namespace NotificationClosed
        {
            static constexpr char const * NAME = "NotificationClosed";
            enum Reason { EXPIRED=1, DISMISSED=2, API=3, UNDEFINED=4 };
        }
    }
}

