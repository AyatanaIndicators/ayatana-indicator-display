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
        g_message("%s getting bus", G_STRLOC);
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
        g_message("%s %s", G_STRLOC, G_STRFUNC);
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
        g_message("%s bus is ready", G_STRLOC);

        m_bus = G_DBUS_CONNECTION(g_object_ref(G_OBJECT(bus)));

        g_dbus_connection_call(m_bus,
                               DBusNames::UnityGreeter::NAME,
                               DBusNames::UnityGreeter::PATH,
                               DBusNames::Properties::INTERFACE,
                               "Get",
                               g_variant_new("(ss)", DBusNames::UnityGreeter::INTERFACE, "IsActive"),
                               G_VARIANT_TYPE("(v)"),
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               m_cancellable,
                               on_get_is_active_ready,
                               this);

        m_subscription_id = g_dbus_connection_signal_subscribe(m_bus,
                                                               DBusNames::UnityGreeter::NAME,
                                                               DBusNames::Properties::INTERFACE,
                                                               DBusNames::Properties::PropertiesChanged::NAME,
                                                               DBusNames::UnityGreeter::PATH,
                                                               DBusNames::UnityGreeter::INTERFACE,
                                                               G_DBUS_SIGNAL_FLAGS_NONE,
                                                               on_properties_changed_signal,
                                                               this,
                                                               nullptr);
    }

    static void on_get_is_active_ready(GObject* source, GAsyncResult* res, gpointer gself)
    {
        g_message("%s", G_STRLOC);
        GError* error {};
        auto v = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), res, &error);
        if (error != nullptr) {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("UsbSnap: Error getting session bus: %s", error->message);
            g_clear_error(&error);
        } else {
            g_message("%s v is %s", G_STRLOC, g_variant_print(v, true));
            GVariant* is_active {};
            g_variant_get_child(v, 0, "v", &is_active);
            static_cast<Impl*>(gself)->m_is_active.set(g_variant_get_boolean(is_active));
            g_clear_pointer(&is_active, g_variant_unref);
        }
        g_clear_pointer(&v, g_variant_unref);
    }

    static void on_properties_changed_signal(GDBusConnection* /*connection*/,
                                             const gchar* /*sender_name*/,
                                             const gchar* object_path,
                                             const gchar* interface_name,
                                             const gchar* signal_name,
                                             GVariant* parameters,
                                             gpointer gself)
    {
        g_message("%s", G_STRLOC);

        g_return_if_fail(!g_strcmp0(object_path, DBusNames::UnityGreeter::PATH));
        g_return_if_fail(!g_strcmp0(interface_name, DBusNames::Properties::INTERFACE));
        g_return_if_fail(!g_strcmp0(signal_name, DBusNames::Properties::PropertiesChanged::NAME));
        g_return_if_fail(g_variant_is_of_type(parameters, G_VARIANT_TYPE(DBusNames::Properties::PropertiesChanged::ARGS_VARIANT_TYPE)));

        auto v = g_variant_get_child_value (parameters, 1);
        if (v != nullptr)
            g_message("%s v is %s", G_STRLOC, g_variant_print(v, true));

        gboolean is_active {};
        if (g_variant_lookup(v, "IsActive", "b", &is_active))
        {
            g_message("%s is_active changed to %d", G_STRLOC, int(is_active));
            static_cast<Impl*>(gself)->m_is_active.set(is_active);
        }
        g_clear_pointer(&v, g_variant_unref);
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

UnityGreeter::UnityGreeter():
    impl{new Impl{}}
{
}

UnityGreeter::~UnityGreeter() =default;

core::Property<bool>&
UnityGreeter::is_active()
{
    return impl->is_active();
}
