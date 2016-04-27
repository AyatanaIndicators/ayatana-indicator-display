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

    Impl()
    {
        m_cancellable.reset(
            g_cancellable_new(),
            [](GCancellable* o){
                g_cancellable_cancel(o);
                g_clear_object(&o);
            }
        );

        g_bus_get(G_BUS_TYPE_SESSION, m_cancellable.get(), on_bus_ready, this);
    }

    ~Impl() =default;

    core::Property<State>& state()
    {
        return m_state;
    }

private:

    void set_state(const State& state)
    {
        m_state.set(state);
    }

    static void on_bus_ready(
        GObject* /*source*/,
        GAsyncResult* res,
        gpointer gself)
    {
        GError* error {};
        auto bus = g_bus_get_finish(res, &error);
        if (error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("Greeter: Error getting bus: %s", error->message);
            g_clear_error(&error);
        }
        else
        {
            auto self = static_cast<Impl*>(gself);

            const auto watcher_id = g_bus_watch_name_on_connection(
                bus,
                DBusNames::UnityGreeter::NAME,
                G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                on_greeter_appeared,
                on_greeter_vanished,
                gself,
                nullptr);

            const auto subscription_id = g_dbus_connection_signal_subscribe(
                bus,
                DBusNames::UnityGreeter::NAME,
                DBusNames::Properties::INTERFACE,
                DBusNames::Properties::PropertiesChanged::NAME,
                DBusNames::UnityGreeter::PATH,
                DBusNames::UnityGreeter::INTERFACE,
                G_DBUS_SIGNAL_FLAGS_NONE,
                on_properties_changed_signal,
                gself,
                nullptr);

            self->m_bus.reset(
                bus,
                [watcher_id, subscription_id](GDBusConnection* o){
                    g_bus_unwatch_name(watcher_id);
                    g_dbus_connection_signal_unsubscribe(o, subscription_id);
                    g_clear_object(&o);
                }
            );
        }
    }

    static void on_greeter_appeared(
        GDBusConnection* bus,
        const char* /*name*/,
        const char* name_owner,
        gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);

        self->m_owner = name_owner;

        g_dbus_connection_call(
            bus,
            DBusNames::UnityGreeter::NAME,
            DBusNames::UnityGreeter::PATH,
            DBusNames::Properties::INTERFACE,
            "Get",
            g_variant_new("(ss)", DBusNames::UnityGreeter::INTERFACE, "IsActive"),
            G_VARIANT_TYPE("(v)"),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            self->m_cancellable.get(),
            on_get_is_active_ready,
            gself);
    }

    static void on_greeter_vanished(
        GDBusConnection* /*bus*/,
        const char* /*name*/,
        gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);

        self->m_owner.clear();
        self->set_state(State::UNAVAILABLE);
    }

    static void on_get_is_active_ready(
        GObject* source,
        GAsyncResult* res, 
        gpointer gself)
    {
        GError* error {};
        auto v = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), res, &error);
        if (error != nullptr) {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("Greeter: Error getting IsActive property: %s", error->message);
            g_clear_error(&error);
        } else {
            GVariant* is_active {};
            g_variant_get_child(v, 0, "v", &is_active);
            static_cast<Impl*>(gself)->set_state(g_variant_get_boolean(is_active) ? State::ACTIVE : State::INACTIVE);
            g_clear_pointer(&is_active, g_variant_unref);
        }
        g_clear_pointer(&v, g_variant_unref);
    }

    static void on_properties_changed_signal(
        GDBusConnection* /*bus*/,
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
        gboolean is_active {};
        if (g_variant_lookup(v, "IsActive", "b", &is_active))
            self->set_state(is_active ? State::ACTIVE : State::INACTIVE);
        g_clear_pointer(&v, g_variant_unref);
    }

    core::Property<State> m_state {State::UNAVAILABLE};
    std::shared_ptr<GCancellable> m_cancellable;
    std::shared_ptr<GDBusConnection> m_bus;
    std::string m_owner;
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

core::Property<Greeter::State>&
UnityGreeter::state()
{
    return impl->state();
}
