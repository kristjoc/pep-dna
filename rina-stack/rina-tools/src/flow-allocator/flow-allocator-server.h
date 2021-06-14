//
// Flow-Allocator Server Application
//
// Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//

#ifndef _FLOW_ALLOCATOR_SERVER_HPP
#define _FLOW_ALLOCATOR_SERVER_HPP

#include <cstdint>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>

#include "application.h"
#include "common.h"

extern int nl_sock;

class FlowAllocatorServer: public Application {
    public:
        FlowAllocatorServer(const std::list<std::string>& _dif_names,
                            const std::string& _server_apn,
                            const std::string& _server_api,
                            bool _app_reg);

        std::thread threads[65535];
        std::thread ev_thread;
        std::mutex fds_lock;
        std::mutex events_lock;
        void ev_thread_fn(void *);
        void insertFds(int, uint32_t);
        int  findFds(int);
        void eraseFds(int);
        void insertEvents(int, void*);
        void *findEvents(int);
        void eraseEvents(int);
        void run(bool);
        ~FlowAllocatorServer();

    protected:
        void destroyFlow(int);

    private:
        std::string dif_name;
        std::string server_apn;
        std::string server_api;
        bool app_reg;
        int nr;
        std::unordered_map<int,uint32_t> fds_map;
        std::unordered_map<int,void*> events_map;
};

#endif
