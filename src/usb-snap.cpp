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
#include <src/usb-snap.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

/***
****
***/

class UsbSnap::Impl
{
public:

    explicit Impl(const std::string& fingerprint):
        m_fingerprint{fingerprint},
        m_cancellable{g_cancellable_new()}
    {
        g_bus_get (G_BUS_TYPE_SESSION, m_cancellable, on_bus_ready_static, this);
    }

    ~Impl()
    {
        g_cancellable_cancel(m_cancellable);
        g_clear_object(&m_cancellable);

        if (m_subscription_id != 0)
            g_dbus_connection_signal_unsubscribe (m_bus, m_subscription_id);

        if (m_notification_id != 0) {
            GError* error {};
            g_dbus_connection_call_sync(m_bus,
                                        DBusNames::Notify::NAME,
                                        DBusNames::Notify::PATH,
                                        DBusNames::Notify::INTERFACE,
                                        "CloseNotification",
                                        g_variant_new("(u)", m_notification_id),
                                        nullptr,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        nullptr,
                                        &error);
            if (error != nullptr) {
                g_warning("Error closing notification: %s", error->message);
                g_clear_error(&error);
            }
        }

        g_clear_object(&m_bus);
    }

    core::Signal<AdbdClient::PKResponse,bool>& on_user_response()
    {
        return m_on_user_response;
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
                                                               DBusNames::Notify::NAME,
                                                               DBusNames::Notify::INTERFACE,
                                                               nullptr,
                                                               DBusNames::Notify::PATH,
                                                               nullptr,
                                                               G_DBUS_SIGNAL_FLAGS_NONE,
                                                               on_notification_signal_static,
                                                               this,
                                                               nullptr);

        auto body = g_strdup_printf(_("The computer's RSA key fingerprint is: %s"), m_fingerprint.c_str());

        GVariantBuilder actions_builder;
        g_variant_builder_init(&actions_builder, G_VARIANT_TYPE_STRING_ARRAY);
        g_variant_builder_add(&actions_builder, "s", ACTION_ALLOW);
        g_variant_builder_add(&actions_builder, "s", _("Allow"));
        g_variant_builder_add(&actions_builder, "s", ACTION_DENY);
        g_variant_builder_add(&actions_builder, "s", _("Don't Allow"));

        GVariantBuilder hints_builder;
        g_variant_builder_init(&hints_builder, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&hints_builder, "{sv}", "x-canonical-non-shaped-icon", g_variant_new_string("true"));
        g_variant_builder_add(&hints_builder, "{sv}", "x-canonical-snap-decisions", g_variant_new_string("true"));
        g_variant_builder_add(&hints_builder, "{sv}", "x-canonical-private-affirmative-tint", g_variant_new_string("true"));

g_message("%s calling notification notify", G_STRLOC);
        auto args = g_variant_new("(susssasa{sv}i)",
                                  "",
                                  uint32_t(0),
                                  "computer-symbolic",
                                  _("Allow USB Debugging?"),
                                  body,
                                  &actions_builder,
                                  &hints_builder,
                                  -1);
        g_dbus_connection_call(m_bus,
                               DBusNames::Notify::NAME,
                               DBusNames::Notify::PATH,
                               DBusNames::Notify::INTERFACE,
                               "Notify",
                               args,
                               G_VARIANT_TYPE("(u)"),
                               G_DBUS_CALL_FLAGS_NONE,
                               -1, // timeout
                               m_cancellable,
                               on_notify_reply_static,
                               this);

        g_clear_pointer(&body, g_free);
    }

    static void on_notify_reply_static(GObject* obus, GAsyncResult* res, gpointer gself)
    {
        GError* error {};
        auto reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION(obus), res, &error);
        if (error != nullptr) {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("UsbSnap: Error calling Notify: %s", error->message);
            g_clear_error(&error);
        } else {
g_message("%s on_notify reply %s", G_STRLOC, g_variant_print(reply, true));
            uint32_t id {};
            g_variant_get(reply, "(u)", &id);
            static_cast<Impl*>(gself)->on_notify_reply(id);
        }
        g_clear_pointer(&reply, g_variant_unref);
    }

    void on_notify_reply(uint32_t id)
    {
g_message("%s setting m_notification_id to %d", G_STRLOC, int(id));
        m_notification_id = id;
    }

    static void on_notification_signal_static(GDBusConnection* /*connection*/,
                                              const gchar* /*sender_name*/,
                                              const gchar* object_path,
                                              const gchar* interface_name,
                                              const gchar* signal_name,
                                              GVariant* parameters,
                                              gpointer gself)
    {
        g_return_if_fail(!g_strcmp0(object_path, DBusNames::Notify::PATH));
        g_return_if_fail(!g_strcmp0(interface_name, DBusNames::Notify::INTERFACE));

        auto self = static_cast<Impl*>(gself);
g_message("%s got signal %s with parameters %s", G_STRLOC, signal_name, g_variant_print(parameters, true));

        if (!g_strcmp0(signal_name, DBusNames::Notify::ActionInvoked::NAME))
        {
            uint32_t id {};
            const char* action_name {};
            g_variant_get(parameters, "(u&s)", &id, &action_name);
            if (id == self->m_notification_id)
                self->on_action_invoked(action_name);
        }
        else if (!g_strcmp0(signal_name, DBusNames::Notify::NotificationClosed::NAME))
        {
            uint32_t id {};
            uint32_t close_reason {};
            g_variant_get(parameters, "(uu)", &id, &close_reason);
            if (id == self->m_notification_id)
                self->on_notification_closed(close_reason);
        }
    }

    void on_action_invoked(const char* action_name)
    {
        const auto response = !g_strcmp0(action_name, ACTION_ALLOW)
            ? AdbdClient::PKResponse::ALLOW
            : AdbdClient::PKResponse::DENY;

        // FIXME: the current default is to cover the most common use case.
        // We need to get the notification ui's checkbox working ASAP so
        // that the user can provide this flag
        const bool remember_this_choice = response == AdbdClient::PKResponse::ALLOW;

        m_on_user_response(response, remember_this_choice);
g_message("%s clearing m_notification_id", G_STRLOC);
        m_notification_id = 0;
    }

    void on_notification_closed(uint32_t close_reason)
    {
g_message("%s closed with reason %d", G_STRLOC, int(close_reason));
        if (close_reason == DBusNames::Notify::NotificationClosed::Reason::EXPIRED)
            m_on_user_response(AdbdClient::PKResponse::DENY, false);

g_message("%s clearing m_notification_id", G_STRLOC);
        m_notification_id = 0;
    }

    static constexpr char const * ACTION_ALLOW {"allow"};
    static constexpr char const * ACTION_DENY  {"deny"};

    const std::string m_fingerprint;
    core::Signal<AdbdClient::PKResponse,bool> m_on_user_response;
    GCancellable* m_cancellable {};
    GDBusConnection* m_bus {};
    uint32_t m_notification_id {};
    unsigned int m_subscription_id {};
};

/***
****
***/

UsbSnap::UsbSnap(const std::string& public_key):
    impl{new Impl{public_key}}
{
}

UsbSnap::~UsbSnap()
{
}

core::Signal<AdbdClient::PKResponse,bool>&
UsbSnap::on_user_response()
{
    return impl->on_user_response();
}

