/*
 * Userspace FlowAllocator app main
 *
 * Adapted by: Kr1stj0n C1k0 <kristjoc AT ifi DOT uio DOT no>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *  */

#include <cstdlib>
#include <string>

#include <librina/librina.h>

#define RINA_PREFIX "fallocator"
#include <librina/logs.h>

#include "tclap/CmdLine.h"

#include "config.h"
#include "flow-allocator-client.h"
#include "flow-allocator-server.h"
#include "utils.h"
#include "common.h"

using namespace std;

volatile sig_atomic_t running = true;
std::condition_variable cv;
int nl_sock  = 0;
pid_t nl_pid = 0;

int wrapped_main(int argc, char** argv)
{
    list<string> dif_names;
    string client_apn;
    string client_api;
    string server_apn;
    string server_api;
    bool   app_reg;
    bool   listen;

    try {
        TCLAP::CmdLine cmd("flow-allocator", ' ', PACKAGE_VERSION);
        TCLAP::SwitchArg listen_arg("l",
                "listen",
                "Run in server mode",
                false);
        TCLAP::SwitchArg registration_arg("r",
                "register",
                "Register the application",
                false);
        TCLAP::ValueArg<string> server_apn_arg("",
                "server-apn",
                "Application process name for the server",
                false,
                "fallocator.server",
                "string");
        TCLAP::ValueArg<string> server_api_arg("",
                "server-api",
                "Application process instance for the server",
                false,
                "",
                "string");
        TCLAP::ValueArg<string> client_apn_arg("",
                "client-apn",
                "Application process name for the client",
                false,
                "fallocator.client",
                "string");
        TCLAP::ValueArg<string> client_api_arg("",
                "client-api",
                "Application process instance for the client",
                false,
                "",
                "string");
        TCLAP::ValueArg<string> dif_arg("d",
                "difs-to-register-at",
                "The names of the DIFs to register at, separated by ',' "
                "(empty means 'any DIF')",
                false,
                "normal.DIF",
                "string");

        cmd.add(listen_arg);
        cmd.add(registration_arg);
        cmd.add(client_apn_arg);
        cmd.add(client_api_arg);
        cmd.add(server_apn_arg);
        cmd.add(server_api_arg);
        cmd.add(dif_arg);

        cmd.parse(argc, argv);

        parse_dif_names(dif_names, dif_arg.getValue());
        client_apn = client_apn_arg.getValue();
        client_api = client_api_arg.getValue();
        server_apn = server_apn_arg.getValue();
        server_api = server_api_arg.getValue();
        app_reg    = registration_arg.getValue();
        listen     = listen_arg.getValue();

    } catch (TCLAP::ArgException &e) {
        LOG_ERR("Error: %s for arg %d", e.error().c_str(),
                e.argId().c_str());
        return EXIT_FAILURE;
    }

    rina::initialize("INFO", "");

    if (listen) {
        /* Server mode */
        FlowAllocatorServer s(dif_names, server_apn, server_api,
                app_reg);

        s.run(false);
    } else {
        /* Client mode */
        FlowAllocatorClient c(dif_names, client_apn, client_api,
                server_apn, server_api, app_reg);

        c.run();
    }

    return EXIT_SUCCESS;
}

int main(int argc, char * argv[])
{
    int retval;

    Signal sig_alrm(SIGALRM, Signal::dummy_handler);
    Signal sig_int(SIGINT, Signal::dummy_handler);

    try {
        retval = wrapped_main(argc, argv);
    } catch (rina::Exception& e) {
        LOG_ERR("%s", e.what());
        return EXIT_FAILURE;

    } catch (std::exception& e) {
        LOG_ERR("Uncaught exception");
        return EXIT_FAILURE;
    }

    return retval;
}
