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

#include <tests/utils/test-dbus-fixture.h>
#include <tests/utils/adbd-server.h>

#include <src/adbd-client.h>

class AdbdClientFixture: public TestDBusFixture
{
private:
    typedef TestDBusFixture super;

protected:

    static void file_deleter (std::string* s)
    {
        fprintf(stderr, "remove \"%s\"\n", s->c_str());
        g_remove(s->c_str());
        delete s;
    }

    std::shared_ptr<std::string> m_tmpdir;

    void SetUp()
    {
        super::SetUp();

        char tmpl[] = {"adb-client-test-XXXXXX"};
        m_tmpdir.reset(new std::string{g_mkdtemp(tmpl)}, file_deleter);
        g_message("using tmpdir '%s'", m_tmpdir->c_str());
    }
};


TEST_F(AdbdClientFixture, SocketPlumbing)
{
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

    const auto main_thread = g_thread_self();

    const auto socket_path = *m_tmpdir + "/test-socket-plumbing";
    g_message("socket_path is %s", socket_path.c_str());

    for (const auto& test : tests)
    {
g_message("%s %s thread %p starting test", G_STRLOC, G_STRFUNC, g_thread_self());
        // start an AdbdClient that listens for PKRequests
        std::string pk;
        auto adbd_client = std::make_shared<GAdbdClient>(socket_path);
        auto connection = adbd_client->on_pk_request().connect([&pk, main_thread, test](const AdbdClient::PKRequest& req){
g_message("%s %s thread %p", G_STRLOC, G_STRFUNC, g_thread_self());
            EXPECT_EQ(main_thread, g_thread_self());
g_message("%s %s thread %p", G_STRLOC, G_STRFUNC, g_thread_self());
            g_message("in on_pk_request with %s", req.public_key.c_str());
g_message("%s %s thread %p", G_STRLOC, G_STRFUNC, g_thread_self());
            pk = req.public_key;
g_message("%s %s thread %p", G_STRLOC, G_STRFUNC, g_thread_self());
            req.respond(test.response);
g_message("%s %s thread %p", G_STRLOC, G_STRFUNC, g_thread_self());
        });

        // start a mock AdbdServer with to fire test key requests and wait for a response
        auto adbd_server = std::make_shared<GAdbdServer>(socket_path, std::vector<std::string>{test.request});
        wait_for([adbd_server](){return !adbd_server->m_responses.empty();}, 5000);
        EXPECT_EQ(test.expected_pk, pk);
        ASSERT_EQ(1, adbd_server->m_responses.size());
        EXPECT_EQ(test.expected_response, adbd_server->m_responses.front());
   
        // cleanup
g_message("%s %s thread %p cleanup", G_STRLOC, G_STRFUNC, g_thread_self());
        connection.disconnect();
        adbd_client.reset();
        adbd_server.reset();
        g_unlink(socket_path.c_str());
g_message("%s %s thread %p test exit", G_STRLOC, G_STRFUNC, g_thread_self());
    }
}
