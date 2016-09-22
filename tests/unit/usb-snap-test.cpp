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

#include <tests/utils/qt-fixture.h>

#include <src/dbus-names.h>
#include <src/usb-snap.h>

#include <libqtdbustest/DBusTestRunner.h>
#include <libqtdbustest/QProcessDBusService.h>
#include <libqtdbusmock/DBusMock.h>

class UsbSnapFixture: public QtFixture
{
    using super = QtFixture;

public:

    UsbSnapFixture():
        dbusMock{dbusTestRunner}
    {
        dbusTestRunner.startServices();
    }

    ~UsbSnapFixture() =default;

protected:

    void SetUp() override
    {
        super::SetUp();

        dbusMock.registerNotificationDaemon();
        dbusTestRunner.startServices();
    }

    OrgFreedesktopDBusMockInterface& notificationsMockInterface()
    {
        return dbusMock.mockInterface(DBusNames::Notify::NAME,
                                      DBusNames::Notify::PATH,
                                      DBusNames::Notify::INTERFACE,
                                      QDBusConnection::SessionBus);
    }

    QtDBusTest::DBusTestRunner dbusTestRunner;
    QtDBusMock::DBusMock dbusMock;
};

TEST_F(UsbSnapFixture, TestRoundTrip)
{
    struct {
        const char* fingerprint;
        const char* action_to_invoke;
        const AdbdClient::PKResponse expected_response;
    } tests[] = {
        { "Fingerprint",  "allow", AdbdClient::PKResponse::ALLOW },
        { "Fingerprint",  "deny",  AdbdClient::PKResponse::DENY }
    };

    uint32_t next_id = 1;
    for(const auto& test : tests)
    {
        // Minor wart: we don't have a way of getting the fdo notification id
        // from dbusmock so instead we copy its (simple) id generation here
        const auto id = next_id++;

        QSignalSpy notificationsSpy(
            &notificationsMockInterface(),
            SIGNAL(MethodCalled(const QString &, const QVariantList &)));

        // start up a UsbSnap to ask about a fingerprint
        auto snap = std::make_shared<UsbSnap>(test.fingerprint);
        AdbdClient::PKResponse user_response {};
        bool user_response_set = false;
        auto connection = snap->on_user_response().connect([&user_response,&user_response_set](AdbdClient::PKResponse response, bool /*remember*/){
            user_response = response;
            user_response_set = true;
        });

        // test that UsbSnap creates a fdo notification
        wait_for_signals(notificationsSpy, 1);
        {
            QVariantList const& call(notificationsSpy.at(0));
            EXPECT_EQ("Notify", call.at(0));

            QVariantList const& args(call.at(1).toList());
            ASSERT_EQ(8, args.size());
            EXPECT_EQ("", args.at(0)); // app name
            EXPECT_EQ(0, args.at(1)); // replaces-id
            EXPECT_EQ("computer-symbolic", args.at(2)); // icon name
            EXPECT_EQ("Allow USB Debugging?", args.at(3)); // summary
            EXPECT_EQ(QString::fromUtf8("The computer's RSA key fingerprint is: ") + test.fingerprint, args.at(4)); // body
            EXPECT_EQ(QStringList({"allow", "Allow", "deny", "Don't Allow"}), args.at(5)); // actions
            EXPECT_EQ(-1, args.at(7));

            QVariantMap hints;
            ASSERT_TRUE(qDBusArgumentToMap(args.at(6), hints));
            ASSERT_EQ(3, hints.size());
            ASSERT_TRUE(hints.contains("x-canonical-private-affirmative-tint"));
            ASSERT_TRUE(hints.contains("x-canonical-non-shaped-icon"));
            ASSERT_TRUE(hints.contains("x-canonical-snap-decisions"));
        }
        notificationsSpy.clear();

        // fake a user interaction with the fdo notification
        notificationsMockInterface().EmitSignal(
            DBusNames::Notify::INTERFACE,
            DBusNames::Notify::ActionInvoked::NAME,
            "us",
            QVariantList() << id << test.action_to_invoke);

        // test that UsbSnap emits on_user_response() as a result
        wait_for([&user_response_set](){return user_response_set;});
        EXPECT_TRUE(user_response_set);
        ASSERT_EQ(test.expected_response, user_response);

        // confirm that the snap dtor cleans up the notification
        connection.disconnect();
        snap.reset();
        wait_for_signals(notificationsSpy, 1);
        {
            QVariantList const& call(notificationsSpy.at(0));
            EXPECT_EQ("CloseNotification", call.at(0));
            QVariantList const& args(call.at(1).toList());
            EXPECT_EQ(id, args.at(0));
        }
    }
}
