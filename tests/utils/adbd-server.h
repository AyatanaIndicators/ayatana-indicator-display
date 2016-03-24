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

#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>

#include <string>
#include <thread>
#include <vector>


/**
 * A Mock ADBD server.
 *
 * Binds to a local domain socket, sends public key requests across it,
 * and reads back the client's responses.
 */
class GAdbdServer
{
public:

    GAdbdServer(const std::string& socket_path,
                const std::vector<std::string>& requests):
        m_requests{requests},
        m_socket_path{socket_path},
        m_cancellable{g_cancellable_new()},
        m_worker_thread{&GAdbdServer::worker_func, this}
    {
    }

    ~GAdbdServer()
    {
        // tell the worker thread to stop whatever it's doing and exit.
        g_cancellable_cancel(m_cancellable);
        m_worker_thread.join();
        g_clear_object(&m_cancellable);
    }

    const std::vector<std::string> m_requests;
    std::vector<std::string> m_responses;

private:

    void worker_func() // runs in worker thread
    {
        auto server_socket = create_server_socket(m_socket_path);
        auto requests = m_requests;

        GError* error {};
        g_socket_listen (server_socket, &error);
        g_assert_no_error (error);

        while (!g_cancellable_is_cancelled(m_cancellable) && !requests.empty())
        {
            // wait for a client connection
            g_message("GAdbdServer::Impl::worker_func() calling g_socket_accept()");
            auto client_socket = g_socket_accept(server_socket, m_cancellable, &error);
            if (error != nullptr) {
                if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                    g_message("GAdbdServer: Error accepting socket connection: %s", error->message);
                g_clear_error(&error);
                break;
            }

            // pop the next request off the stack
            auto request = requests.front();

            // send the request
            g_message("GAdbdServer::Impl::worker_func() sending req [%s]", request.c_str());
            g_socket_send(client_socket,
                          request.c_str(),
                          request.size(),
                          m_cancellable,
                          &error);
            if (error != nullptr) {
                if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                    g_message("GAdbdServer: Error sending request: %s", error->message);
                g_clear_error(&error);
                g_clear_object(&client_socket);
                break;
            }

            // read the response
            g_message("GAdbdServer::Impl::worker_func() reading response");
            char buf[4096];
            const auto n_bytes = g_socket_receive(client_socket,
                                                  buf,
                                                  sizeof(buf),
                                                  m_cancellable,
                                                  &error);
            if (error != nullptr) {
                if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                    g_message("GAdbdServer: Error reading response: %s", error->message);
                g_clear_error(&error);
                g_clear_object(&client_socket);
                continue;
            }
            const std::string response(buf, std::string::size_type(n_bytes));
            g_message("server read %d bytes, got response: '%s'", int(n_bytes), response.c_str()); 
            if (!response.empty()) {
                m_responses.push_back(response);
                requests.erase(requests.begin());
            }

            // cleanup
            g_clear_object(&client_socket);
        }

        g_clear_object(&server_socket);
    }

    // bind to a local domain socket
    static GSocket* create_server_socket(const std::string& socket_path)
    {
        GError* error {};
        auto socket = g_socket_new(G_SOCKET_FAMILY_UNIX,
                                   G_SOCKET_TYPE_STREAM,
                                   G_SOCKET_PROTOCOL_DEFAULT,
                                   &error);
        g_assert_no_error(error);
        auto address = g_unix_socket_address_new (socket_path.c_str());
        g_socket_bind (socket, address, false, &error);
        g_assert_no_error (error);
        g_clear_object (&address);

        return socket;
    }

    const std::string m_socket_path;
    GCancellable* m_cancellable = nullptr;
    std::thread m_worker_thread;
};


