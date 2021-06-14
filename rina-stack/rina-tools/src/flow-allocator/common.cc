/*
 * Flow-Allocator common source
 *
 * Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
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


#include "common.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/socket.h>

#define RINA_PREFIX "fallocator.common"

#include <librina/logs.h>

#include <cstdio>
#include <cstdlib>

Signal::Signal(int signum, sighandler_t handler) : signum(signum)
{
    struct sigaction act;
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(signum, &act, &oldact);
}

void Signal::dummy_handler(int) {
    running = false;
    cv.notify_all();
}

Signal::~Signal() {
    sigaction(signum, &oldact, NULL);
}

int netlink_init(void)
{
    struct nl_msg nlmsg = {0};
    int snd = NL_SNDBUF_DEF;
    int rcv = NL_RCVBUF_DEF;
    int sock = 0, rc = 0;

    nl_pid = getpid();

    sock = socket(PF_NETLINK, SOCK_RAW, NL_PEPDNA_PROTO);
    if (sock < 0) {
        LOG_ERR("socket");
        return -1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &snd, sizeof(int))) {
        LOG_ERR("Unable to set Netlink socket buffer send size");
        return -1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(int))) {
        LOG_ERR("Unable to set socket buffer receive size");
        return -1;
    }

    /* Upon creating nl_sock, send an empty msg to kernel so that it can
     * register flow-allocator nl-pid */
    rc = netlink_send_data(sock, &nlmsg);
    if (rc < 0) {
        LOG_ERR("sendmsg inside init");
        close(sock);
        return -1;
    }

    return sock;
}

int netlink_send_data(int sock, struct nl_msg *nlmsg)
{
    struct nlmsghdr *nlh = nullptr;
    struct sockaddr_nl daddr;
    struct msghdr msg = {0};
    struct iovec iov;

    nlh = (struct nlmsghdr *)calloc(1, NLMSG_SPACE(NETLINK_MSS));
    if (!nlh) {
        LOG_ERR("calloc nlh");
        return -ENOMEM;
    }
    nlh->nlmsg_len    = NLMSG_SPACE(NETLINK_MSS);
    nlh->nlmsg_pid    = nl_pid;    /* Sender PID */
    nlh->nlmsg_flags  = 0;

    memset(&daddr, 0, sizeof(daddr));
    daddr.nl_family = AF_NETLINK;
    daddr.nl_pid    = 0;   /* To Linux Kernel */
    daddr.nl_groups = 0;   /* unicast */


    void *data = (void *)nlmsg;
    memcpy(NLMSG_DATA(nlh), data, sizeof(struct nl_msg));
    memset(&iov, 0, sizeof(iov));
    iov.iov_base     = (void *)nlh;
    iov.iov_len      = NLMSG_SPACE(NETLINK_MSS);

    memset(&msg, 0, sizeof(msg));
    msg.msg_name     = (void *)&daddr;
    msg.msg_namelen  = sizeof(daddr);
    msg.msg_iov      = &iov;
    msg.msg_iovlen   = 1;

    if (sendmsg(sock, &msg, MSG_DONTWAIT) < 0) {
        LOG_ERR("netlink_sendmsg %d", errno);
        free(nlh); nlh = nullptr;
        return -1;
    }

    free(nlh); nlh = nullptr;
    return 0;
}

uint32_t cstring_to_uint32(const char *ip)
{
    int a, b, c, d;
    char arr[4];

    sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d);
    arr[0] = a; arr[1] = b; arr[2] = c; arr[3] = d;

    uint32_t *tmp = (uint32_t *)arr;
    return *tmp;
}

uint16_t cstring_to_uint16(const char *port)
{
    uint16_t rc;

    if(port == NULL)
        return 0;
    long int val = strtol(port, 0, 10);
    if(val < 0 || val > UINT16_MAX)
        return 0;

    rc = (uint16_t)val;

    return rc;
}

std::string uint32_to_string(uint32_t int_ip)
{
    char cstr_ip[16];

    sprintf(cstr_ip, "%d.%d.%d.%d", (int_ip) & 0xFF,
                                    (int_ip >> 8) & 0xFF,
                                    (int_ip >> 16) & 0xFF,
                                    (int_ip >> 24) & 0xFF);

    std::string str(cstr_ip);

    return str;
}
