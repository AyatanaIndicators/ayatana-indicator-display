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

class UsbManager::Impl
{
public:
 
    explicit Impl(
        const std::string& socket_path,
        const std::string& public_keys_filename
    ):
        m_adbd_client{std::make_shared<GAdbdClient>(socket_path)},
        m_public_keys_filename{public_keys_filename}
    {
        m_adbd_client->on_pk_request().connect([this](const AdbdClient::PKRequest& req){
            auto snap = new UsbSnap(req.fingerprint);
            snap->on_user_response().connect([this,req,snap](AdbdClient::PKResponse response, bool remember_choice){
                g_debug("%s user responded! response %d, remember %d", G_STRLOC, int(response), int(remember_choice));
                req.respond(response);
                if (remember_choice && (response == AdbdClient::PKResponse::ALLOW))
                    write_public_key(req.public_key);
                // delete_later
                g_idle_add([](gpointer gsnap){delete static_cast<UsbSnap*>(gsnap); return G_SOURCE_REMOVE;}, snap);
            });
        });
    }

    ~Impl()
    {
    }

private:

    void write_public_key(const std::string& public_key)
    {
        g_debug("writing public key '%s' to '%s'", public_key.c_str(), m_public_keys_filename.c_str());

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
            g_warning("Error opening ADB datafile '%s': %s", m_public_keys_filename.c_str(), g_strerror(errno));
            return;
        }

        // write the new public key on its own line
        const std::string buf {public_key + '\n'};
        if (write(fd, buf.c_str(), buf.size()) == -1)
            g_warning("Error writing ADB datafile: %d %s", errno, g_strerror(errno));
        close(fd);
    }

    std::shared_ptr<GAdbdClient> m_adbd_client;
    const std::string m_public_keys_filename;
};

/***
****
***/

UsbManager::UsbManager(
    const std::string& socket_path,
    const std::string& public_keys_filename
):
    impl{new Impl{socket_path, public_keys_filename}}
{
}

UsbManager::~UsbManager()
{
}

