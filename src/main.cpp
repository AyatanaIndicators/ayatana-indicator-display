/*
 * Copyright 2014 Canonical Ltd.
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

#include <src/exporter.h>
#include <src/rotation-lock.h>
#include <src/usb-manager.h>

#include <glib/gi18n.h> // bindtextdomain()
#include <gio/gio.h>

#include <locale.h>

int
main(int /*argc*/, char** /*argv*/)
{
#warning NB the next line turns on verbose debug logging and is used for developement. Remove it before landing.
g_assert(g_setenv("G_MESSAGES_DEBUG", "all", true));

    // Work around a deadlock in glib's type initialization.
    // It can be removed when https://bugzilla.gnome.org/show_bug.cgi?id=674885 is fixed.
    g_type_ensure(G_TYPE_DBUS_CONNECTION);

    // boilerplate i18n
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
    textdomain(GETTEXT_PACKAGE);

    auto loop = g_main_loop_new(nullptr, false);
    auto on_name_lost = [loop](const std::string& name){
      g_warning("busname lost: '%s'", name.c_str());
      g_main_loop_quit(loop);
    };

    // build all our indicators.
    // Right now we've only got one -- rotation lock -- but hey, we can dream.
    std::vector<std::shared_ptr<Indicator>> indicators;
    std::vector<std::shared_ptr<Exporter>> exporters;
    indicators.push_back(std::make_shared<RotationLockIndicator>());
    for (auto& indicator : indicators) {
      auto exporter = std::make_shared<Exporter>(indicator);
      exporter->name_lost().connect(on_name_lost);
      exporters.push_back(exporter);
    }

    // We need the ADBD handler running,
    // even though it doesn't have an indicator component yet
    static constexpr char const * ADB_SOCKET_PATH {"/dev/socket/adbd"};
    static constexpr char const * PUBLIC_KEYS_FILENAME {"/data/misc/adb/adb_keys"};
    UsbManager usb_manager {ADB_SOCKET_PATH, PUBLIC_KEYS_FILENAME};

    // let's go!
    g_main_loop_run(loop);

    // cleanup
    g_main_loop_unref(loop);
    return 0;
}
