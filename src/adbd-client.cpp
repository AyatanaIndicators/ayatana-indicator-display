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

#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

class GAdbdClient::Impl
{
public:

    explicit Impl(const std::string& socket_path):
        m_socket_path{socket_path},
        m_cancellable{g_cancellable_new()},
        m_worker_thread{&Impl::worker_func, this}
    {
    }

    ~Impl()
    {
        // tell the worker thread to stop whatever it's doing and exit.
        g_debug("%s Client::Impl dtor, cancelling m_cancellable", G_STRLOC);
        g_cancellable_cancel(m_cancellable);
        m_pkresponse_cv.notify_one();
        m_sleep_cv.notify_one();
        m_worker_thread.join();
        g_clear_object(&m_cancellable);
    }

    core::Signal<const PKRequest&>& on_pk_request()
    {
        return m_on_pk_request;
    }

private:

    // struct to carry request info from the worker thread to the GMainContext thread
    struct PKIdleData
    {
        Impl* self = nullptr;
        GCancellable* cancellable = nullptr;
        const std::string public_key;

        PKIdleData(Impl* self_, GCancellable* cancellable_, const std::string& public_key_):
            self(self_),
            cancellable(G_CANCELLABLE(g_object_ref(cancellable_))),
            public_key(public_key_) {}

        ~PKIdleData() {g_clear_object(&cancellable);}
        
    };

    void pass_public_key_to_main_thread(const std::string& public_key)
    {
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                        on_public_key_request_static,
                        new PKIdleData{this, m_cancellable, public_key},
                        [](gpointer id){delete static_cast<PKIdleData*>(id);});
    }

    static gboolean on_public_key_request_static (gpointer gdata) // runs in main thread
    {
        /* NB: It's possible (though unlikely) that data.self was destroyed
           while this callback was pending, so we must check is-cancelled FIRST */
        auto data = static_cast<PKIdleData*>(gdata);
        if (!g_cancellable_is_cancelled(data->cancellable))
        {
            // notify our listeners of the request
            auto self = data->self;
            struct PKRequest req;
            req.public_key = data->public_key;
            req.fingerprint = get_fingerprint(req.public_key);
            req.respond = [self](PKResponse response){self->on_public_key_response(response);};
            self->m_on_pk_request(req);
        }

        return G_SOURCE_REMOVE;
    }

    void on_public_key_response(PKResponse response)
    {
        // set m_pkresponse and wake up the waiting worker thread
        std::unique_lock<std::mutex> lk(m_pkresponse_mutex);
        m_pkresponse = response;
        m_pkresponse_ready = true;
        m_pkresponse_cv.notify_one();
    }

    /***
    ****
    ***/

    void worker_func() // runs in worker thread
    {
        const std::string socket_path {m_socket_path};

        while (!g_cancellable_is_cancelled(m_cancellable))
        {
            g_debug("%s creating a client socket to '%s'", G_STRLOC, socket_path.c_str());
            auto socket = create_client_socket(socket_path);
            bool got_valid_req = false;

            g_debug("%s calling read_request", G_STRLOC);
            std::string reqstr;
            if (socket != nullptr)
                reqstr = read_request(socket);
            if (!reqstr.empty())
                g_debug("%s got request [%s]", G_STRLOC, reqstr.c_str());

            if (reqstr.substr(0,2) == "PK") {
                PKResponse response = PKResponse::DENY;
                const auto public_key = reqstr.substr(2);
                g_debug("%s got pk [%s]", G_STRLOC, public_key.c_str());
                if (!public_key.empty()) {
                    got_valid_req = true;
                    std::unique_lock<std::mutex> lk(m_pkresponse_mutex);
                    m_pkresponse_ready = false;
                    pass_public_key_to_main_thread(public_key);
                    m_pkresponse_cv.wait(lk, [this](){
                        return m_pkresponse_ready || g_cancellable_is_cancelled(m_cancellable);
                    });
                    response = m_pkresponse;
                    g_debug("%s got response '%d', is-cancelled %d", G_STRLOC,
                            int(response),
                            int(g_cancellable_is_cancelled(m_cancellable)));
                }
                if (!g_cancellable_is_cancelled(m_cancellable))
                    send_pk_response(socket, response);
            } else if (!reqstr.empty()) {
                g_warning("Invalid ADB request: [%s]", reqstr.c_str());
            }

            g_clear_object(&socket);

            // If nothing interesting's happening, sleep a bit.
            // (Interval copied from UsbDebuggingManager.java)
            static constexpr std::chrono::seconds sleep_interval {std::chrono::seconds(1)};
            if (!got_valid_req && !g_cancellable_is_cancelled(m_cancellable)) {
                std::unique_lock<std::mutex> lk(m_sleep_mutex);
                m_sleep_cv.wait_for(lk, sleep_interval);
            }
        }
    }

    // connect to a local domain socket
    GSocket* create_client_socket(const std::string& socket_path)
    {
        GError* error {};
        auto socket = g_socket_new(G_SOCKET_FAMILY_UNIX,
                                   G_SOCKET_TYPE_STREAM,
                                   G_SOCKET_PROTOCOL_DEFAULT,
                                   &error);
        if (error != nullptr) {
            g_warning("Error creating adbd client socket: %s", error->message);
            g_clear_error(&error);
            g_clear_object(&socket);
            return nullptr;
        }

        auto address = g_unix_socket_address_new(socket_path.c_str());
        const auto connected = g_socket_connect(socket, address, m_cancellable, &error);
        g_clear_object(&address);
        if (!connected) {
            g_debug("unable to connect to '%s': %s", socket_path.c_str(), error->message);
            g_clear_error(&error);
            g_clear_object(&socket);
            return nullptr;
        }

        return socket;
    }

    std::string read_request(GSocket* socket)
    {
        char buf[4096] = {};
        g_debug("%s calling g_socket_receive()", G_STRLOC);
        const auto n_bytes = g_socket_receive (socket, buf, sizeof(buf), m_cancellable, nullptr);
        std::string ret;
        if (n_bytes > 0)
            ret.append(buf, std::string::size_type(n_bytes));
        g_debug("%s g_socket_receive got %d bytes: [%s]", G_STRLOC, int(n_bytes), ret.c_str());
        return ret;
    }

    void send_pk_response(GSocket* socket, PKResponse response)
    {
        std::string response_str;
        switch(response) {
            case PKResponse::ALLOW: response_str = "OK"; break;
            case PKResponse::DENY:  response_str = "NO"; break;
        }
        g_debug("%s sending reply: [%s]", G_STRLOC, response_str.c_str());

        GError* error {};
        g_socket_send(socket,
                      response_str.c_str(),
                      response_str.size(),
                      m_cancellable,
                      &error);
        if (error != nullptr) {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("GAdbdServer: Error accepting socket connection: %s", error->message);
            g_clear_error(&error);
        }
    }

    static std::string get_fingerprint(const std::string& public_key)
    {
        // The first token is base64-encoded data, so cut on the first whitespace
        const std::string base64 (
            public_key.begin(),
            std::find_if(
                public_key.begin(), public_key.end(),
                [](const std::string::value_type& ch){return std::isspace(ch);}
            )
        );

        gsize digest_len {};
        auto digest = g_base64_decode(base64.c_str(), &digest_len);

        auto checksum = g_compute_checksum_for_data(G_CHECKSUM_MD5, digest, digest_len);
        const gsize checksum_len = checksum ? strlen(checksum) : 0;

        // insert ':' between character pairs; eg "ff27b5f3" --> "ff:27:b5:f3"
        std::string fingerprint;
        for (gsize i=0; i<checksum_len; ) {
            fingerprint.append(checksum+i, checksum+i+2);
            if (i < checksum_len-2)
                fingerprint.append(":");
            i += 2;
        }

        g_clear_pointer(&digest, g_free);
        g_clear_pointer(&checksum, g_free);
        return fingerprint;
    }

    const std::string m_socket_path;
    GCancellable* m_cancellable = nullptr;
    std::thread m_worker_thread;
    core::Signal<const PKRequest&> m_on_pk_request;

    std::mutex m_sleep_mutex;
    std::condition_variable m_sleep_cv;

    std::mutex m_pkresponse_mutex;
    std::condition_variable m_pkresponse_cv;
    bool m_pkresponse_ready = false;
    PKResponse m_pkresponse = PKResponse::DENY;
};

/***
****
***/

AdbdClient::~AdbdClient()
{
}

/***
****
***/

GAdbdClient::GAdbdClient(const std::string& socket_path):
    impl{new Impl{socket_path}}
{
}

GAdbdClient::~GAdbdClient()
{
}

core::Signal<const AdbdClient::PKRequest&>&
GAdbdClient::on_pk_request()
{
    return impl->on_pk_request();
}

