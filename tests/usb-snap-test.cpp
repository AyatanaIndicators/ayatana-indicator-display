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
#include <tests/dbus-types.h>
#include <tests/glib-fixture.h>

#include <src/usb-snap.h>

#include <glib.h>

#include <libqtdbustest/DBusTestRunner.h>
#include <libqtdbustest/QProcessDBusService.h>
#include <libqtdbusmock/DBusMock.h>

#include <QSignalSpy>

#include <gtest/gtest.h>

using namespace QtDBusTest;
using namespace QtDBusMock;

inline QString qVariantToString(const QVariant& variant) {
    QString output;
    QDebug dbg(&output);
    dbg << variant;
    return output;
}

inline void PrintTo(const QVariant& variant, std::ostream* os) {
    QString output;
    QDebug dbg(&output);
    dbg << variant;

    *os << "QVariant(" << output.toStdString() << ")";
}

inline void PrintTo(const QString& s, std::ostream* os) {
    *os << "\"" << s.toStdString() << "\"";
}

inline void PrintTo(const QStringList& list, std::ostream* os) {
    QString output;
    QDebug dbg(&output);
    dbg << list;

    *os << "QStringList(" << output.toStdString() << ")";
}

inline void PrintTo(const QList<QDBusObjectPath>& list, std::ostream* os) {
    QString output;
    for (const auto& path: list)
    {
        output.append("\"" + path.path() + "\",");
    }

    *os << "QList<QDBusObjectPath>(" << output.toStdString() << ")";
}

#define WAIT_FOR_SIGNALS(signalSpy, signalsExpected)\
{\
    while (signalSpy.size() < signalsExpected)\
    {\
        ASSERT_TRUE(signalSpy.wait());\
    }\
    ASSERT_EQ(signalsExpected, signalSpy.size());\
}

class UsbSnapFixture: public GlibFixture
{
    using super = GlibFixture;

public:

    UsbSnapFixture():
        dbusMock{dbusTestRunner}
    {
        DBusTypes::registerMetaTypes();

        dbusTestRunner.startServices();
    }

    ~UsbSnapFixture() =default;

protected:

    bool qDBusArgumentToMap(QVariant const& variant, QVariantMap& map)
    {
        if (variant.canConvert<QDBusArgument>())
        {
            QDBusArgument value(variant.value<QDBusArgument>());
            if (value.currentType() == QDBusArgument::MapType)
            {
                value >> map;
                return true;
            }
        }
        return false;
    }

    void SetUp() override
    {
        super::SetUp();

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
        snap->on_user_response().connect([&user_response,&user_response_set](AdbdClient::PKResponse response, bool /*remember*/){
            user_response = response;
            user_response_set = true;
        });

        // test that UsbSnap creates a fdo notification
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
            EXPECT_EQ(QString::fromUtf8("The computer's RSA key fingerprint is: ") + test.fingerprint, args.at(4)); // body
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

        // fake a user interaction with the fdo notification
        notificationsMockInterface().EmitSignal(
            DBusTypes::NOTIFY_DBUS_INTERFACE,
            "ActionInvoked",
            "us",
            QVariantList() << id << test.action_to_invoke);

        // test that UsbSnap emits on_user_response() as a result
        wait_for([&user_response_set](){return user_response_set;});
        EXPECT_TRUE(user_response_set);
        ASSERT_EQ(test.expected_response, user_response);

        // confirm that the snap dtor cleans up the notification
        snap.reset();
        WAIT_FOR_SIGNALS(notificationsSpy, 1);
        {
            QVariantList const& call(notificationsSpy.at(0));
            EXPECT_EQ("CloseNotification", call.at(0));
            QVariantList const& args(call.at(1).toList());
            EXPECT_EQ(id, args.at(0));
        }
    }
}
