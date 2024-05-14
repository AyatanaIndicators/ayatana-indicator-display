/*
 * Copyright 2014 Canonical Ltd.
 * Copyright 2022-2023 Robert Tari
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
 *   Robert Tari <robert@tari.in>
 */

#include <src/exporter.h>
#include <src/service.h>

#ifdef LOMIRI_FEATURES_ENABLED
#include <src/greeter.h>
#include <src/usb-manager.h>
#include <src/usb-monitor.h>
#include <sys/stat.h>
#include <errno.h>
#endif

#include <glib/gi18n.h> // bindtextdomain()
#include <gio/gio.h>
#include <locale.h>

extern "C"
{
    #include <ayatana/common/utils.h>
}

int
main(int /*argc*/, char** /*argv*/)
{
    // Work around a deadlock in glib's type initialization.
    // It can be removed when https://bugzilla.gnome.org/show_bug.cgi?id=674885 is fixed.
    g_type_ensure(G_TYPE_DBUS_CONNECTION);

    // boilerplate i18n
    setlocale(LC_ALL, "");

    // Initialize LC_NUMERIC with 'POSIX'. This assures that float number
    // conversions (from float to string via e.g. g_strdup_sprintf()) always
    // use a dot in decimal numbers.
    //
    // This resolves blackening of the screen if users with e.g. de_DE.UTF-8
    // use the brightness slider and hand over a komma-decimal to the xsct
    // executable (which only understands dot-decimals).
    //
    // As we don't use numbers / number conversions anywhere else in the
    // display indicator, this global setting of LC_NUMERIC seems to be the
    // easiest approach.
    setlocale(LC_NUMERIC, "POSIX");

    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    textdomain(GETTEXT_PACKAGE);

    auto loop = g_main_loop_new(nullptr, false);
    auto on_name_lost = [loop](const std::string& name){
      g_warning("busname lost: '%s'", name.c_str());
      g_main_loop_quit(loop);
    };

    std::vector<std::shared_ptr<Indicator>> indicators;
    std::vector<std::shared_ptr<Exporter>> exporters;
    indicators.push_back(std::make_shared<DisplayIndicator>());
    for (auto& indicator : indicators) {
      auto exporter = std::make_shared<Exporter>(indicator);
      exporter->name_lost().connect(on_name_lost);
      exporters.push_back(exporter);
    }

    // let's go!
    g_main_loop_run(loop);

    // cleanup
    g_main_loop_unref(loop);
    return 0;
}
