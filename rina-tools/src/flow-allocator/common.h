/*
 * Flow-Allocator common header
 *
 * Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
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
 */

#ifndef _FLOW_ALLOCATOR_COMMON_HPP
#define _FLOW_ALLOCATOR_COMMON_HPP

#include <string>
#include <stdint.h>
#include <signal.h>
#include <condition_variable>    /* std::condition_variable */

#define NL_PEPDNA_PROTO 31
#define NETLINK_MSS 21
#define MAX_CONNS 65535
#define NL_RCVBUF_DEF 8388608
#define NL_SNDBUF_DEF 8388608

extern volatile sig_atomic_t running;
extern std::condition_variable cv;
extern pid_t nl_pid;

struct nl_msg {
    uint32_t saddr;
    uint16_t source;
    uint32_t daddr;
    uint16_t dest;
    uint32_t hash_conn_id;
    int port_id;
    bool alloc;
} __attribute__((packed));

class Signal {
        public:
            Signal(int signum, sighandler_t handler);
            ~Signal();
            static void dummy_handler(int);
        private:
            int signum;
            struct sigaction oldact;
};

int netlink_init(void);
int netlink_send_data(int, struct nl_msg *);
uint32_t cstring_to_uint32(const char *);
uint16_t cstring_to_uint16(const char *);
std::string uint32_to_string(uint32_t);

#endif
