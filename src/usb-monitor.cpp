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

#include <src/usb-monitor.h>

#include <glib.h>
#include <gudev/gudev.h>

class GUDevUsbMonitor::Impl
{
public:
 
    Impl()
    {
        const char* subsystems[] = {"android_usb", nullptr};
        m_udev_client = g_udev_client_new(subsystems);
        g_signal_connect(m_udev_client, "uevent", G_CALLBACK(on_android_usb_event), this);
    }

    ~Impl()
    {
        g_signal_handlers_disconnect_by_data(m_udev_client, this);
        g_clear_object(&m_udev_client);
    }

    core::Signal<const std::string&>& on_usb_disconnected()
    {
        return m_on_usb_disconnected;
    }

private:

    static void on_android_usb_event(GUdevClient*, gchar* action, GUdevDevice* device, gpointer gself)
    {
        if (!g_strcmp0(action, "change"))
            if (!g_strcmp0(g_udev_device_get_property(device, "USB_STATE"), "DISCONNECTED"))
                static_cast<Impl*>(gself)->m_on_usb_disconnected(g_udev_device_get_name(device));
    }

    core::Signal<const std::string&> m_on_usb_disconnected;

    GUdevClient* m_udev_client = nullptr;
};

/***
****
***/

UsbMonitor::UsbMonitor() =default;

UsbMonitor::~UsbMonitor() =default;

GUDevUsbMonitor::GUDevUsbMonitor():
    impl{new Impl{}}
{
}

GUDevUsbMonitor::~GUDevUsbMonitor() =default;

core::Signal<const std::string&>&
GUDevUsbMonitor::on_usb_disconnected()
{
    return impl->on_usb_disconnected();
}

