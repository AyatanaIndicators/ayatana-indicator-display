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

#include <src/usb-monitor.h>

class MockUsbMonitor: public UsbMonitor
{
public:
    MockUsbMonitor() =default;
    virtual ~MockUsbMonitor() =default;
    core::Signal<const std::string&>& on_usb_disconnected() override {return m_on_usb_disconnected;}
    core::Signal<const std::string&> m_on_usb_disconnected;
};

