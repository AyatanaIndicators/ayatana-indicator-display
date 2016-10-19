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

#include <src/adbd-client.h>
#include <src/usb-manager.h>
#include <src/usb-snap.h>

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <set>

class UsbManager::Impl
{
public:
 
    explicit Impl(
        const std::string& socket_path,
        const std::string& public_keys_filename,
        const std::shared_ptr<UsbMonitor>& usb_monitor,
        const std::shared_ptr<Greeter>& greeter
    ):
        m_socket_path{socket_path},
        m_public_keys_filename{public_keys_filename},
        m_usb_monitor{usb_monitor},
        m_greeter{greeter}
    {
        m_usb_monitor->on_usb_disconnected().connect([this](const std::string& /*usb_name*/) {
            m_req.reset();
        });

        m_greeter->state().changed().connect([this](const Greeter::State& state) {
            if (state == Greeter::State::INACTIVE)
                maybe_snap();
            else
                stop_snap();
        });

        // create a new adbd client
        m_adbd_client.reset(new GAdbdClient{m_socket_path});
        m_adbd_client->on_pk_request().connect(
            [this](const AdbdClient::PKRequest& req) {
                g_debug("%s got pk request: %s, calling maybe_snap()", G_STRLOC, req.fingerprint.c_str());

                m_response = AdbdClient::PKResponse::DENY; // set the fallback response
                m_req.reset(
                    new AdbdClient::PKRequest(req),
                    [this](AdbdClient::PKRequest* r) {
                        stop_snap();
                        r->respond(m_response);
                        delete r;
                    }
                );
                maybe_snap();
            }
        );
    }

    ~Impl()
    {
        if (m_request_complete_idle_tag)
            g_source_remove(m_request_complete_idle_tag);
    }

private:

    void stop_snap()
    {
        m_snap_connections.clear();
        m_snap.reset();
    }

    void maybe_snap()
    {
        // only prompt if there's something to prompt about
        if (!m_req)
            return;

        // only prompt in an unlocked session
        if (m_greeter->state().get() != Greeter::State::INACTIVE)
            return;

        snap();
    }

    void snap()
    {
        m_snap = std::make_shared<UsbSnap>(m_req->fingerprint);
        m_snap_connections.insert((*m_snap).on_user_response().connect(
            [this](AdbdClient::PKResponse response, bool remember_choice){

                if (remember_choice && (response == AdbdClient::PKResponse::ALLOW))
                    write_public_key(m_req->public_key);

                m_response = response;

                // defer finishing the request into an idle func because
                // ScopedConnections can't be destroyed inside their callbacks
                if (m_request_complete_idle_tag == 0) {
                    m_request_complete_idle_tag = g_idle_add([](gpointer gself){
                        auto self = static_cast<Impl*>(gself);
                        self->m_request_complete_idle_tag = 0;
                        self->m_req.reset();
                        return G_SOURCE_REMOVE;
                    }, this);
                }
            }
        ));
    }

    void write_public_key(const std::string& public_key)
    {
        g_debug("%s writing public key '%s' to '%s'", G_STRLOC, public_key.c_str(), m_public_keys_filename.c_str());

        // confirm the directory exists
        auto dirname = g_path_get_dirname(m_public_keys_filename.c_str());
        const auto dir_exists = g_file_test(dirname, G_FILE_TEST_IS_DIR);
        if (!dir_exists)
            g_warning("ADB data directory '%s' does not exist", dirname);
        g_clear_pointer(&dirname, g_free);
        if (!dir_exists)
            return;

        // open the file in append mode, with user rw and group r permissions
        const auto fd = open(
            m_public_keys_filename.c_str(),
            O_APPEND|O_CREAT|O_WRONLY,
            S_IRUSR|S_IWUSR|S_IRGRP
        );
        if (fd == -1) {
            g_warning("Error opening ADB datafile: %s", g_strerror(errno));
            return;
        }

        // write the new public key on its own line
        std::string buf {public_key + '\n'};
        if (write(fd, buf.c_str(), buf.size()) == -1)
            g_warning("Error writing ADB datafile: %d %s", errno, g_strerror(errno));
        close(fd);
    }

    const std::string m_socket_path;
    const std::string m_public_keys_filename;
    const std::shared_ptr<UsbMonitor> m_usb_monitor;
    const std::shared_ptr<Greeter> m_greeter;
 
    unsigned int m_request_complete_idle_tag {};

    std::shared_ptr<GAdbdClient> m_adbd_client;
    std::shared_ptr<UsbSnap> m_snap;
    std::set<core::ScopedConnection> m_snap_connections;
    AdbdClient::PKResponse m_response {AdbdClient::PKResponse::DENY};
    std::shared_ptr<AdbdClient::PKRequest> m_req;
};

/***
****
***/

UsbManager::UsbManager(
    const std::string& socket_path,
    const std::string& public_keys_filename,
    const std::shared_ptr<UsbMonitor>& usb_monitor,
    const std::shared_ptr<Greeter>& greeter
):
    impl{new Impl{socket_path, public_keys_filename, usb_monitor, greeter}}
{
}

UsbManager::~UsbManager()
{
}
