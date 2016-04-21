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
#include <src/greeter.h>

#include <libqtdbustest/DBusTestRunner.h>
#include <libqtdbustest/QProcessDBusService.h>
#include <libqtdbusmock/DBusMock.h>

class GreeterFixture: public QtFixture
{
private:

    using super = QtFixture;

public:

    GreeterFixture() =default;
    ~GreeterFixture() =default;

protected:

    std::shared_ptr<QtDBusTest::DBusTestRunner> m_dbus_runner;
    std::shared_ptr<QtDBusMock::DBusMock> m_dbus_mock;
    GDBusConnection* m_session_bus {};

    void SetUp() override
    {
        super::SetUp();
  
        // use a fresh bus for each test
 
        m_dbus_runner.reset(new QtDBusTest::DBusTestRunner());
        m_dbus_mock.reset(new QtDBusMock::DBusMock(*m_dbus_runner.get()));

        GError* error {};
        m_session_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        g_assert_no_error(error);
        g_dbus_connection_set_exit_on_close(m_session_bus, false);
    }

    void TearDown() override
    {
        g_clear_object(&m_session_bus);
        m_dbus_mock.reset();
        m_dbus_runner.reset();

        super::TearDown();
    }

    void start_greeter_service(bool is_active)
    {
        // set a watcher to look for our mock to appear
        bool owned {};
        QDBusServiceWatcher watcher(
            DBusNames::UnityGreeter::NAME,
            m_dbus_runner->sessionConnection()
        );
        QObject::connect(
            &watcher,
            &QDBusServiceWatcher::serviceRegistered,
            [&owned](const QString&){owned = true;}
        );

        // start the mock greeter
        QVariantMap parameters;
        parameters["IsActive"] = QVariant(is_active);
        m_dbus_mock->registerTemplate(
            DBusNames::UnityGreeter::NAME,
            GREETER_TEMPLATE,
            parameters,
            QDBusConnection::SessionBus
        );
        m_dbus_runner->startServices();

        // wait for the watcher to be triggered
        ASSERT_TRUE(wait_for([&owned]{return owned;}));
    }
};

#define ASSERT_PROPERTY_EVENTUALLY(expected_in, property_in) \
    do { \
        const auto& e = expected_in; \
        const auto& p = property_in; \
        ASSERT_TRUE(wait_for([e, &p](){ return e == p.get(); })) \
            << "expected " << e << " but got " << p.get(); \
   } while(0)

TEST_F(GreeterFixture, ActiveServiceStartsBeforeWatcher)
{
    constexpr bool expected {true};

    start_greeter_service(expected);

    UnityGreeter greeter;

    ASSERT_PROPERTY_EVENTUALLY(expected, greeter.is_active());
}

TEST_F(GreeterFixture, WatcherStartsBeforeActiveService)
{
    constexpr bool expected {true};

    UnityGreeter greeter;

    start_greeter_service(expected);

    ASSERT_PROPERTY_EVENTUALLY(expected, greeter.is_active());
}

TEST_F(GreeterFixture, InactiveServiceStartsBeforeWatcher)
{
    constexpr bool expected {false};

    start_greeter_service(expected);

    UnityGreeter greeter;

    ASSERT_PROPERTY_EVENTUALLY(expected, greeter.is_active());
}

TEST_F(GreeterFixture, WatcherStartsBeforeInactiveService)
{
    constexpr bool expected {false};

    UnityGreeter greeter;

    start_greeter_service(expected);

    ASSERT_PROPERTY_EVENTUALLY(expected, greeter.is_active());
}

