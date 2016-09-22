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
g_debug("%s %s", G_STRLOC, G_STRFUNC);
    }

    ~Impl()
    {
g_debug("%s %s DTOR DTOR dtor", G_STRLOC, G_STRFUNC);
        // tell the worker thread to stop whatever it's doing and exit.
        g_debug("%s Client::Impl dtor, cancelling m_cancellable", G_STRLOC);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        g_cancellable_cancel(m_cancellable);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        m_pkresponse_cv.notify_one();
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        m_sleep_cv.notify_one();
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        if (m_worker_thread.joinable())
            m_worker_thread.join();
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        g_clear_object(&m_cancellable);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
    }

    core::Signal<const PKRequest&>& on_pk_request()
    {
g_debug("%s %s thread %p", G_STRLOC, G_STRFUNC, g_thread_self());
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
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                        on_public_key_request_static,
                        new PKIdleData{this, m_cancellable, public_key},
                        [](gpointer id){delete static_cast<PKIdleData*>(id);});
    }

    static gboolean on_public_key_request_static (gpointer gdata) // runs in main thread
    {
        /* NB: It's possible (though unlikely) that data.self was destroyed
           while this callback was pending, so we must check is-cancelled FIRST */
g_debug("%s %s thread is %p", G_STRLOC, G_STRFUNC, g_thread_self());
        auto data = static_cast<PKIdleData*>(gdata);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        if (!g_cancellable_is_cancelled(data->cancellable))
        {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            // notify our listeners of the request
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            auto self = data->self;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            struct PKRequest req;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            req.public_key = data->public_key;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            req.fingerprint = get_fingerprint(req.public_key);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            req.respond = [self](PKResponse response){self->on_public_key_response(response);};
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            self->m_on_pk_request(req);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        }

g_debug("%s %s", G_STRLOC, G_STRFUNC);
        return G_SOURCE_REMOVE;
    }

    void on_public_key_response(PKResponse response)
    {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        g_debug("%s thread %p got response %d", G_STRLOC, g_thread_self(), int(response));

g_debug("%s %s", G_STRLOC, G_STRFUNC);
        // set m_pkresponse and wake up the waiting worker thread
        std::lock_guard<std::mutex> lk(m_pkresponse_mutex);
        //std::unique_lock<std::mutex> lk(m_pkresponse_mutex);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        m_pkresponse = response;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        m_pkresponse_ready = true;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        m_pkresponse_cv.notify_one();
g_debug("%s %s", G_STRLOC, G_STRFUNC);
    }

    /***
    ****
    ***/

    void worker_func() // runs in worker thread
    {
g_debug("%s %s worker thread is %p", G_STRLOC, G_STRFUNC, g_thread_self());
        const std::string socket_path {m_socket_path};

g_debug("%s %s", G_STRLOC, G_STRFUNC);
        while (!g_cancellable_is_cancelled(m_cancellable))
        {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            g_debug("%s thread %p creating a client socket to '%s'", G_STRLOC, g_thread_self(), socket_path.c_str());
            auto socket = create_client_socket(socket_path);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            bool got_valid_req = false;

            g_debug("%s thread %p calling read_request", g_thread_self(), G_STRLOC);
            std::string reqstr;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            if (socket != nullptr)
                reqstr = read_request(socket);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            if (!reqstr.empty())
                g_debug("%s got request [%s]", G_STRLOC, reqstr.c_str());

g_debug("%s %s", G_STRLOC, G_STRFUNC);
            if (reqstr.substr(0,2) == "PK") {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                PKResponse response = PKResponse::DENY;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                const auto public_key = reqstr.substr(2);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                g_debug("%s thread %p got pk [%s]", G_STRLOC, g_thread_self(), public_key.c_str());
                if (!public_key.empty()) {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                    got_valid_req = true;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                    std::unique_lock<std::mutex> lk(m_pkresponse_mutex);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                    m_pkresponse_ready = false;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                    m_pkresponse = AdbdClient::PKResponse::DENY;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                    pass_public_key_to_main_thread(public_key);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                    g_debug("%s thread %p waiting", G_STRLOC, g_thread_self());
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                    try {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                        m_pkresponse_cv.wait(lk, [this](){
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                            return m_pkresponse_ready || g_cancellable_is_cancelled(m_cancellable);
                        });
                    } catch (std::system_error& e) {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                        g_critical("%s thread %p unable to wait for response because of unexpected error '%s'", G_STRLOC, g_thread_self(), e.what());
                    }
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                    response = m_pkresponse;
                    g_debug("%s thread %p got response '%d', is-cancelled %d", G_STRLOC,
                            g_thread_self(),
                            int(response),
                            int(g_cancellable_is_cancelled(m_cancellable)));
                }
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                if (!g_cancellable_is_cancelled(m_cancellable))
                    send_pk_response(socket, response);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            } else if (!reqstr.empty()) {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                g_warning("Invalid ADB request: [%s]", reqstr.c_str());
            }

g_debug("%s %s", G_STRLOC, G_STRFUNC);
            g_clear_object(&socket);

            // If nothing interesting's happening, sleep a bit.
            // (Interval copied from UsbDebuggingManager.java)
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            static constexpr std::chrono::seconds sleep_interval {std::chrono::seconds(1)};
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            if (!got_valid_req && !g_cancellable_is_cancelled(m_cancellable)) {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                std::unique_lock<std::mutex> lk(m_sleep_mutex);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                m_sleep_cv.wait_for(lk, sleep_interval);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            }
        }
    }

    // connect to a local domain socket
    GSocket* create_client_socket(const std::string& socket_path)
    {
        GError* error {};
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        auto socket = g_socket_new(G_SOCKET_FAMILY_UNIX,
                                   G_SOCKET_TYPE_STREAM,
                                   G_SOCKET_PROTOCOL_DEFAULT,
                                   &error);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        if (error != nullptr) {
            g_warning("Error creating adbd client socket: %s", error->message);
            g_clear_error(&error);
            g_clear_object(&socket);
            return nullptr;
        }

g_debug("%s %s", G_STRLOC, G_STRFUNC);
        auto address = g_unix_socket_address_new(socket_path.c_str());
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        const auto connected = g_socket_connect(socket, address, m_cancellable, &error);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        g_clear_object(&address);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        if (!connected) {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            g_debug("unable to connect to '%s': %s", socket_path.c_str(), error->message);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            g_clear_error(&error);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            g_clear_object(&socket);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            return nullptr;
        }

g_debug("%s %s", G_STRLOC, G_STRFUNC);
        return socket;
    }

    std::string read_request(GSocket* socket)
    {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        char buf[4096] = {};
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        g_debug("%s calling g_socket_receive()", G_STRLOC);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        const auto n_bytes = g_socket_receive (socket, buf, sizeof(buf), m_cancellable, nullptr);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        std::string ret;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        if (n_bytes > 0)
            ret.append(buf, std::string::size_type(n_bytes));
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        g_debug("%s g_socket_receive got %d bytes: [%s]", G_STRLOC, int(n_bytes), ret.c_str());
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        return ret;
    }

    void send_pk_response(GSocket* socket, PKResponse response)
    {
        std::string response_str;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        switch(response) {
            case PKResponse::ALLOW: response_str = "OK"; break;
            case PKResponse::DENY:  response_str = "NO"; break;
        }
        g_debug("%s sending reply: [%s]", G_STRLOC, response_str.c_str());

g_debug("%s %s", G_STRLOC, G_STRFUNC);
        GError* error {};
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        g_socket_send(socket,
                      response_str.c_str(),
                      response_str.size(),
                      m_cancellable,
                      &error);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        if (error != nullptr) {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
g_debug("%s %s", G_STRLOC, G_STRFUNC);
                g_warning("GAdbdServer: Error accepting socket connection: %s", error->message);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
            g_clear_error(&error);
        }
g_debug("%s %s", G_STRLOC, G_STRFUNC);
    }

    static std::string get_fingerprint(const std::string& public_key)
    {
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        // The first token is base64-encoded data, so cut on the first whitespace
        const std::string base64 (
            public_key.begin(),
            std::find_if(
                public_key.begin(), public_key.end(),
                [](const std::string::value_type& ch){return std::isspace(ch);}
            )
        );

g_debug("%s %s", G_STRLOC, G_STRFUNC);
        gsize digest_len {};
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        auto digest = g_base64_decode(base64.c_str(), &digest_len);
g_debug("%s %s", G_STRLOC, G_STRFUNC);

        auto checksum = g_compute_checksum_for_data(G_CHECKSUM_MD5, digest, digest_len);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        const gsize checksum_len = checksum ? strlen(checksum) : 0;
g_debug("%s %s", G_STRLOC, G_STRFUNC);

        // insert ':' between character pairs; eg "ff27b5f3" --> "ff:27:b5:f3"
        std::string fingerprint;
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        for (gsize i=0; i<checksum_len; ) {
            fingerprint.append(checksum+i, checksum+i+2);
            if (i < checksum_len-2)
                fingerprint.append(":");
            i += 2;
        }

g_debug("%s %s", G_STRLOC, G_STRFUNC);
        g_clear_pointer(&digest, g_free);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
        g_clear_pointer(&checksum, g_free);
g_debug("%s %s", G_STRLOC, G_STRFUNC);
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

