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

#define QT_NO_KEYWORDS

#include <tests/utils/adbd-server.h>
#include <tests/utils/dbus-types.h>
#include <tests/utils/glib-fixture.h>
#include <tests/utils/gtest-qt-print-helpers.h>
#include <tests/utils/qdbus-helpers.h>

#include <src/usb-manager.h>

#include <libqtdbustest/DBusTestRunner.h>
#include <libqtdbustest/QProcessDBusService.h>
#include <libqtdbusmock/DBusMock.h>

#include <glib.h>

#include <gtest/gtest.h>

#include <QSignalSpy>

#include <fstream>
#include <sstream>
#include <vector>

/***
****
***/

#define WAIT_FOR_SIGNALS(signalSpy, signalsExpected)\
{\
    while (signalSpy.size() < signalsExpected)\
    {\
        ASSERT_TRUE(signalSpy.wait());\
    }\
    ASSERT_EQ(signalsExpected, signalSpy.size());\
}

class UsbManagerFixture: public GlibFixture
{
    using super = GlibFixture;

public:

    UsbManagerFixture():
        dbusMock{dbusTestRunner}
    {
        DBusTypes::registerMetaTypes();

        dbusTestRunner.startServices();
    }

    ~UsbManagerFixture() =default;

protected:

    static void file_deleter (std::string* s)
    {
        fprintf(stderr, "remove \"%s\"\n", s->c_str());
        remove(s->c_str());
        delete s;
    }

    void SetUp() override
    {
        super::SetUp();

        char tmpl[] = {"usb-manager-test-XXXXXX"};
        m_tmpdir.reset(new std::string{g_mkdtemp(tmpl)}, file_deleter);
        g_message("using tmpdir '%s'", m_tmpdir->c_str());

        dbusMock.registerNotificationDaemon();
        dbusTestRunner.startServices();
    }

    OrgFreedesktopDBusMockInterface& notificationsMockInterface()
    {
        return dbusMock.mockInterface("org.freedesktop.Notifications",
                                      "/org/freedesktop/Notifications",
                                      "org.freedesktop.Notifications",
                                       QDBusConnection::SessionBus);
    }

    QtDBusTest::DBusTestRunner dbusTestRunner;
    QtDBusMock::DBusMock dbusMock;
    QtDBusTest::DBusServicePtr indicator;
    std::shared_ptr<std::string> m_tmpdir;
};

TEST_F(UsbManagerFixture, Allow)
{
    const std::shared_ptr<std::string> socket_path {new std::string{*m_tmpdir+"/socket"}, file_deleter};
    const std::shared_ptr<std::string> public_keys_path {new std::string{*m_tmpdir+"/adb_keys"}, file_deleter};

    // add a signal spy to listen to the notification daemon
    QSignalSpy notificationsSpy(
        &notificationsMockInterface(),
        SIGNAL(MethodCalled(const QString &, const QVariantList &))
    );

    // start a mock AdbdServer ready to submit a request
    const std::string public_key {"qAAAALUHllFjEZjl5jbS9ivjpQpaTNpibl28Re71D/S8sV3usNJTkbpvZYoVPfxtmHSNdCgLkWN6qcDZsHZqE/4myzmx/8Y/RqBy1oirudugi3YUUcJh7aWkY8lKQe9shCLTcrT7cFLZIJIidTvfmWTm0UcU+xmdPALze11I3lGo1Ty5KpCe9oP+qYM8suHbxhm78LKLlo0QJ2QqM8T5isr1pvoPHDgRb+mSESElG+xDIfPWA2BTu77/xk4EnXmOYfcuCr5akF3N4fRo/ACnYgXWDZFX2XdklBXyDj78lVlinF37xdMk7BMQh166X7UNkpH1uG2y5F6lUzyLg8SsFtRnJkw7eVe/gnJj3feQaFQbF5oVDhWhLMtWLtejhX6umvroVBVA4rynG4xEgs00K4u4ly8DUIIJYDO22Ml4myFR5CUm3lOlyitNdzYGh0utLXPq9oc8EbMVxM3i+O7PRxQw5Ul04X6K8GLiGUDV98DB+xYUqfEveq1BRnXi/ZrdPDhQ8Lfkg5xnLccPTFamAqutPtZXV6s7dXJInBTZf0NtBaWL0RdR2cOJBrpeBYkrc9yIyeqFLFdxr66rjaehjaa4pS4S+CD6PkGiIpPWSQtwNC4RlT10qTQ0/K9lRux2p0D8Z8ubUTFuh4kBScGUkN1OV3Z+7d7B+ghmBtZrrgleXsbehjRuKgEAAQA= foo@bar"};
    const std::string fingerprint {"12:23:5f:2d:8c:40:ae:1d:05:7b:ae:bd:88:8a:f0:80"};
    auto adbd_server = std::make_shared<GAdbdServer>(*socket_path, std::vector<std::string>{"PK"+public_key});

    // set up a UsbManager to process the request
    auto usb_manager = std::make_shared<UsbManager>(*socket_path, *public_keys_path);

    // wait for the notification to show up, confirm it looks right
    WAIT_FOR_SIGNALS(notificationsSpy, 1);
    {
        QVariantList const& call(notificationsSpy.at(0));
        EXPECT_EQ("Notify", call.at(0));

        QVariantList const& args(call.at(1).toList());
        ASSERT_EQ(8, args.size());
        EXPECT_EQ("", args.at(0)); // app name
        EXPECT_EQ(0, args.at(1)); // replaces-id
        EXPECT_EQ("computer-symbolic", args.at(2)); // icon name
        EXPECT_EQ("Allow USB Debugging?", args.at(3)); // summary
        EXPECT_EQ(QString::fromUtf8("The computer's RSA key fingerprint is: ") + QString::fromUtf8(fingerprint.c_str()), args.at(4)); // body
        EXPECT_EQ(QStringList({"allow", "Allow", "deny", "Deny"}), args.at(5)); // actions
        EXPECT_EQ(-1, args.at(7));

        QVariantMap hints;
        ASSERT_TRUE(qDBusArgumentToMap(args.at(6), hints));
        ASSERT_EQ(3, hints.size());
        ASSERT_TRUE(hints.contains("x-canonical-private-affirmative-tint"));
        ASSERT_TRUE(hints.contains("x-canonical-non-shaped-icon"));
        ASSERT_TRUE(hints.contains("x-canonical-snap-decisions"));
    }
    notificationsSpy.clear();

    // click on allow in the notification
    notificationsMockInterface().EmitSignal(
        DBusTypes::NOTIFY_DBUS_INTERFACE,
        "ActionInvoked",
        "us",
        QVariantList() << uint32_t(1) << "allow"
    );

    // confirm that the AdbdServer got the right response
    wait_for([adbd_server](){return !adbd_server->m_responses.empty();}, 2000);
    ASSERT_EQ(1, adbd_server->m_responses.size());
    EXPECT_EQ("OK", adbd_server->m_responses.front());

    // confirm that the public_keys file got the public key appended to it
    std::ifstream ifkeys {*public_keys_path};
    std::vector<std::string> lines;
    std::string line;
    while(getline(ifkeys, line))
        lines.emplace_back(std::move(line));
    ASSERT_EQ(1, lines.size());
    EXPECT_EQ(public_key, lines[0]);
}
