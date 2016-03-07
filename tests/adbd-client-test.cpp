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

#include <tests/test-dbus-fixture.h>
#include <tests/adbd-server.h>

#include <src/adbd-client.h>

#include <stdlib.h> // mkdtemp

class AdbdClientFixture: public TestDBusFixture
{
private:
    typedef TestDBusFixture super;

protected:

    std::string m_tmpdir;

    void SetUp()
    {
        super::SetUp();

        char tmpl[] {"adb-client-test-XXXXXX"};
        m_tmpdir = mkdtemp(tmpl);
        g_message("using tmpdir '%s'", m_tmpdir.c_str());
    }

    void TearDown()
    {
        g_rmdir(m_tmpdir.c_str());

        super::TearDown();
    }
};


TEST_F(AdbdClientFixture, SocketPlumbing)
{
    const auto socket_path = m_tmpdir + "/test-socket-plumbing";
    g_message("socket_path is %s", socket_path.c_str());

    struct {
        const std::string request;
        const std::string expected_pk;
        AdbdClient::PKResponse response;
        const std::string expected_response;
    } tests[] = {
        { "PKHelloWorld", "HelloWorld", AdbdClient::PKResponse::ALLOW, "OK" },
        { "PKHelloWorld", "HelloWorld", AdbdClient::PKResponse::DENY,  "NO" },
        { "PKFooBar",     "FooBar",     AdbdClient::PKResponse::ALLOW, "OK" },
        { "PK",           "",           AdbdClient::PKResponse::DENY,  "NO" }
    };

    for (const auto& test : tests)
    {
        // make the AdbdClient and start listening for Requests
        std::string pk;
        auto adbd_client = std::make_shared<GAdbdClient>(socket_path);
        adbd_client->on_pk_request().connect([&pk, test](const AdbdClient::PKRequest& req){
            g_message("in on_pk_request with %s", req.public_key.c_str());
            pk = req.public_key;
            req.respond(test.response);
        });

        // fire up a mock ADB server with a preloaded request, and wait for a response
        auto adbd_server = std::make_shared<GAdbdServer>(socket_path, std::vector<std::string>{test.request});
        wait_for([adbd_server](){return !adbd_server->m_responses.empty();}, 2000);
        EXPECT_EQ(test.expected_pk, pk);
        ASSERT_EQ(1, adbd_server->m_responses.size());
        EXPECT_EQ(test.expected_response, adbd_server->m_responses.front());
   
        // cleanup
        adbd_client.reset();
        adbd_server.reset();
        g_unlink(socket_path.c_str());
    }
}
