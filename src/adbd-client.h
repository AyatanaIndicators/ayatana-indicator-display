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

#include <functional>
#include <memory>
#include <string>

#include <core/signal.h>

/**
 * Receives public key requests from ADBD and sends a response back.
 *
 * AdbClient only provides a receive/respond mechanism. The decision
 * of what response gets sent is delegated out to a listener via
 * the on_pk_request signal.
 *
 * The decider should connect to on_pk_request, listen for PKRequests,
 * and call the request's `respond' method with the desired response.
 */
class AdbdClient
{
public:
    virtual ~AdbdClient();

    enum class PKResponse { DENY, ALLOW };

    struct PKRequest {
        std::string public_key;
        std::string fingerprint;
        std::function<void(PKResponse)> respond;
    };

    virtual core::Signal<const PKRequest&>& on_pk_request() =0;

protected:
    AdbdClient() =default;
};

/**
 * An AdbdClient designed to work with GLib's event loop.
 *
 * The on_pk_request() signal will be called in global GMainContext's thread;
 * ie, just like a function passed to g_idle_add() or g_timeout_add().
 */
class GAdbdClient: public AdbdClient
{
public:
    explicit GAdbdClient(const std::string& socket_path);
    ~GAdbdClient();
    core::Signal<const PKRequest&>& on_pk_request() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

