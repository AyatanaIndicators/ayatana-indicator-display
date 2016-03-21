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

#include <src/dbus-names.h>
#include <src/greeter.h>

#include <gio/gio.h>

class UnityGreeter::Impl
{
public:

    Impl():
        m_cancellable{g_cancellable_new()}
    {
        g_bus_get(G_BUS_TYPE_SESSION, m_cancellable, on_bus_ready_static, this);
    }

    ~Impl()
    {
        g_cancellable_cancel(m_cancellable);
        g_clear_object(&m_cancellable);

        if (m_subscription_id != 0)
            g_dbus_connection_signal_unsubscribe (m_bus, m_subscription_id);

        g_clear_object(&m_bus);
    }

    core::Property<bool>& is_active()
    {
        return m_is_active;
    }

private:

    static void on_bus_ready_static(GObject* /*source*/, GAsyncResult* res, gpointer gself)
    {
        GError* error {};
        auto bus = g_bus_get_finish (res, &error);
        if (error != nullptr) {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("UsbSnap: Error getting session bus: %s", error->message);
            g_clear_error(&error);
        } else {
            static_cast<Impl*>(gself)->on_bus_ready(bus);
        }
        g_clear_object(&bus);
    }

    void on_bus_ready(GDBusConnection* bus)
    {
        m_bus = G_DBUS_CONNECTION(g_object_ref(G_OBJECT(bus)));

        m_subscription_id = g_dbus_connection_signal_subscribe(m_bus,
                                                               DBusNames::UnityGreeter::NAME,
                                                               "org.freedesktop.DBus.Properties",
                                                               "PropertiesChanged",
                                                               DBusNames::UnityGreeter::PATH,
                                                               nullptr,
                                                               G_DBUS_SIGNAL_FLAGS_NONE,
                                                               on_properties_changed_signal_static,
                                                               this,
                                                               nullptr);
    }

    static void on_properties_changed_signal_static(GDBusConnection* /*connection*/,
                                                    const gchar* sender_name,
                                                    const gchar* object_path,
                                                    const gchar* interface_name,
                                                    const gchar* signal_name,
                                                    GVariant* parameters,
                                                    gpointer gself)
    {
        g_return_if_fail(!g_strcmp0(sender_name, DBusNames::UnityGreeter::NAME));
        g_return_if_fail(!g_strcmp0(object_path, DBusNames::UnityGreeter::PATH));
        g_return_if_fail(!g_strcmp0(interface_name, "org.freedesktop.DBus.Properties"));
        g_return_if_fail(!g_strcmp0(signal_name, "PropertiesChanged"));

        static_cast<Impl*>(gself)->on_properties_changed_signal(parameters);
    }

    void on_properties_changed_signal(GVariant* parameters)
    {
        g_message("%s %s", G_STRLOC, g_variant_print(parameters, true));
    }

    core::Property<bool> m_is_active;
    GCancellable* m_cancellable {};
    GDBusConnection* m_bus {};
    unsigned int m_subscription_id {};
};

/***
****
***/

Greeter::Greeter() =default;

Greeter::~Greeter() =default;

UnityGreeter::~UnityGreeter() =default;

UnityGreeter::UnityGreeter():
    impl{new Impl{}}
{
}
