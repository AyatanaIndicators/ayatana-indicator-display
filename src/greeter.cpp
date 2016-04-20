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

    explicit Impl(GDBusConnection* connection):
        m_bus(G_DBUS_CONNECTION(g_object_ref(connection))),
        m_cancellable{g_cancellable_new()}
    {
g_message("%s %s", G_STRLOC, G_STRFUNC);

        m_watcher_id = g_bus_watch_name_on_connection(
            m_bus,
            DBusNames::UnityGreeter::NAME,
            G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
            on_greeter_appeared,
            on_greeter_vanished,
            this,
            nullptr);

        m_subscription_id = g_dbus_connection_signal_subscribe(
            m_bus,
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

    ~Impl()
    {
        g_cancellable_cancel(m_cancellable);
        g_clear_object(&m_cancellable);
        g_bus_unwatch_name(m_watcher_id);
        g_dbus_connection_signal_unsubscribe(m_bus, m_subscription_id);
        g_clear_object(&m_bus);
    }

    core::Property<bool>& is_active()
    {
        return m_is_active;
    }

private:

    static void on_greeter_appeared(
        GDBusConnection* /*session_bus*/,
        const char* /*name*/,
        const char* name_owner,
        gpointer gself)
    {
        g_message("%s %s", G_STRLOC, G_STRFUNC);
        auto self = static_cast<Impl*>(gself);

        self->m_owner = name_owner;

        g_dbus_connection_call(
            self->m_bus,
            DBusNames::UnityGreeter::NAME,
            DBusNames::UnityGreeter::PATH,
            DBusNames::Properties::INTERFACE,
            "Get",
            g_variant_new("(ss)", DBusNames::UnityGreeter::INTERFACE, "IsActive"),
            G_VARIANT_TYPE("(v)"),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            self->m_cancellable,
            on_get_is_active_ready,
            gself);
    }

    static void on_greeter_vanished(
        GDBusConnection* /*session_bus*/,
        const char* /*name*/,
        gpointer gself)
    {
        g_message("%s %s", G_STRLOC, G_STRFUNC);
        auto self = static_cast<Impl*>(gself);

        self->m_owner.clear();
        self->m_is_active.set(false);
    }

    static void on_get_is_active_ready(
        GObject* source,
        GAsyncResult* res, 
        gpointer gself)
    {
        g_message("%s", G_STRLOC);
        GError* error {};
        auto v = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), res, &error);
        if (error != nullptr) {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("Greeter: Error getting IsActive property: %s", error->message);
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

    static void on_properties_changed_signal(
        GDBusConnection* /*connection*/,
        const gchar* sender_name,
        const gchar* object_path,
        const gchar* interface_name,
        const gchar* signal_name,
        GVariant* parameters,
        gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);

        g_return_if_fail(!g_strcmp0(sender_name, self->m_owner.c_str()));
        g_return_if_fail(!g_strcmp0(object_path, DBusNames::UnityGreeter::PATH));
        g_return_if_fail(!g_strcmp0(interface_name, DBusNames::Properties::INTERFACE));
        g_return_if_fail(!g_strcmp0(signal_name, DBusNames::Properties::PropertiesChanged::NAME));
        g_return_if_fail(g_variant_is_of_type(parameters, G_VARIANT_TYPE(DBusNames::Properties::PropertiesChanged::ARGS_VARIANT_TYPE)));

        auto v = g_variant_get_child_value(parameters, 1);
        if (v != nullptr)
            g_message("%s v is %s", G_STRLOC, g_variant_print(v, true));

        gboolean is_active {};
        if (g_variant_lookup(v, "IsActive", "b", &is_active))
        {
            g_message("%s is_active changed to %d", G_STRLOC, int(is_active));
            self->m_is_active.set(is_active);
        }
        g_clear_pointer(&v, g_variant_unref);

    }

    core::Property<bool> m_is_active {false};

    GDBusConnection* m_bus {};
    GCancellable* m_cancellable {};
    guint m_watcher_id {};
    unsigned int m_subscription_id {};
    std::string m_owner;
};

/***
****
***/

Greeter::Greeter() =default;

Greeter::~Greeter() =default;

UnityGreeter::UnityGreeter(GDBusConnection* connection):
    impl{new Impl{connection}}
{
}

UnityGreeter::~UnityGreeter() =default;

core::Property<bool>&
UnityGreeter::is_active()
{
    return impl->is_active();
}
