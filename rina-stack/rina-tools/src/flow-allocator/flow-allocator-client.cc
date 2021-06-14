/*
 * Client-Flow-Allocator
 *
 * Adapted from IRATI stack by Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
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
 */

#include <cstring>
#include <deque>
#include <iostream>
#include <pthread.h>
#include <sstream>
#include <cassert>
#include <unistd.h>
#include <limits.h>
#include <iomanip>
#include <cerrno>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <poll.h>

#define RINA_PREFIX "fallocator.client"
#include <librina/ipc-api.h>
#include <librina/logs.h>

#include "flow-allocator-client.h"
#include "common.h"

using namespace std;
using namespace rina;


FlowAllocatorClient::FlowAllocatorClient(const list<string>& _dif_names,
        const string& _client_apn,
        const string& _client_api,
        const string& _server_apn,
        const string& _server_api,
        bool _app_reg) :
    Application(_dif_names, _client_apn, _client_api),
    dif_name(_dif_names.front()),
    client_apn(_client_apn),
    client_api(_client_api),
    server_apn(_server_apn),
    server_api(_server_api),
    app_reg(_app_reg),
    nr(0)
{
    /* Do nothing inside constructor */
}

void FlowAllocatorClient::insertFd(uint32_t _conn_id, int _port_id)
{
    std::lock_guard<std::mutex> guard(fd_lock);

    if (fd_map.size() > MAX_CONNS) {
        LOG_ERR("Max. sessions reached");
        return;
    }
    fd_map.insert({_conn_id, _port_id});

    LOG_DBG("Fd{%u, %d} added by thread %lu", _conn_id, _port_id, pthread_self());
}

void FlowAllocatorClient::insertEvent(unsigned int _seqnum, void *_event)
{
    std::unique_lock<std::mutex> lck(event_lock);

    IPCEvent *evt = (IPCEvent *)_event;
    event_map.insert({_seqnum, evt});

    cv.notify_all();

    LOG_DBG("Event_Map{%d, %p} added by thread %lu", _seqnum, evt,
                                                      pthread_self());
}

int FlowAllocatorClient::findFd(uint32_t _conn_id)
{
    std::unordered_map<uint32_t,int>::const_iterator it = fd_map.find(_conn_id);

    if (it == fd_map.end()) {
        LOG_ERR("Fd{%u, ?} not found", _conn_id);
        return -1;
    } else
        return it->second;
}

void* FlowAllocatorClient::findEvent(unsigned int _seqnum)
{
    std::unordered_map<unsigned int,void*>::const_iterator it = event_map.find(_seqnum);

    if (it == event_map.end()) {
        LOG_ERR("Event_Map{%d, ?} not found", _seqnum);
        return NULL;
    } else {
        LOG_DBG("Event_Map{%d, ?} found by thread %lu", _seqnum, pthread_self());
        return (IPCEvent *)(it->second);
    }
}

void FlowAllocatorClient::eraseFd(uint32_t _conn_id)
{
    std::lock_guard<std::mutex> guard(fd_lock);

    if (findFd(_conn_id) > 0)
        fd_map.erase(_conn_id);
    else
        LOG_ERR("Fd{%u, ?} to be deleted not found", _conn_id);
}

void FlowAllocatorClient::eraseEvent(unsigned int _seqnum)
{
    std::unique_lock<std::mutex> lck(event_lock);

    if (findEvent(_seqnum))
        event_map.erase(_seqnum);
    else
        LOG_ERR("Event_Map{%d, ?} to be deleted not found", _seqnum);
}

void FlowAllocatorClient::thread_fn(struct nl_msg nlmsg)
{
    int port_id = 0;

    if (nlmsg.alloc) {

        LOG_DBG("thread %lu is allocating Fd{%u, ?}", pthread_self(),
                                                      nlmsg.hash_conn_id);
        if ((port_id = createFlow(nlmsg.saddr, nlmsg.source,
                                  nlmsg.daddr, nlmsg.dest,
                                  nlmsg.hash_conn_id)) < 0) {
            LOG_ERR("thread %lu couldn't allocate Flow", pthread_self());
            return;
        }
        insertFd(nlmsg.hash_conn_id, port_id);
        nlmsg.port_id = port_id;

        if (netlink_send_data(nl_sock, &nlmsg) < 0 ) {
            LOG_ERR("ERROR | sendmsg");
            return;
        }
    } else {
        LOG_DBG("thread %lu deallocating Fd{%u, ?}", pthread_self(),
                                                     nlmsg.hash_conn_id);
        destroyFlow(nlmsg.hash_conn_id);
    }
    LOG_DBG("thread %lu terminated", pthread_self());
}

void FlowAllocatorClient::run()
{
    struct nl_msg *data  = nullptr;
    struct nlmsghdr *nlh = nullptr;
    struct msghdr msg;
    struct iovec iov;
    int rc = 0;

    if (app_reg) {
        applicationRegister();
    }

    /* 1. Init Netlink socket */
    nl_sock = netlink_init();
    if (nl_sock < 0) {
        LOG_ERR("Cannot init netlink socket");
        return;
    }

    /* 2. Prepare to recv data from Netlink socket */
    nlh = (struct nlmsghdr *)calloc(1, NLMSG_SPACE(NETLINK_MSS));
    if (!nlh) {
        LOG_ERR("calloc nlh");
        goto out;
    }

    while (running) {
        memset(nlh, 0, NLMSG_SPACE(NETLINK_MSS));
        memset(&iov, 0, sizeof(iov));
        iov.iov_base = (void *)nlh;
        iov.iov_len = NLMSG_SPACE(NETLINK_MSS);

        memset(&msg, 0, sizeof(msg));
        msg.msg_iov    = &iov;
        msg.msg_iovlen = 1;

        rc = recvmsg(nl_sock, &msg, 0);
        if (rc > 0) {
            data = (struct nl_msg *)NLMSG_DATA(nlh);
            threads[nr++] = std::thread(&FlowAllocatorClient::thread_fn, this, *data);
        } else if (rc < 0) {
            if (errno == EAGAIN)
                continue;
            LOG_ERR("netlink recvmsg %d", errno);
            goto out;
        } else { /* rc = 0 */
            LOG_ERR("netlink recvmsg %d", errno);
            goto out;
        }
    }
out:
    cv.notify_all();
    free(nlh); nlh = nullptr;
    close(nl_sock);
}

int FlowAllocatorClient::createFlow(uint32_t saddr,
                                    uint16_t source,
                                    uint32_t daddr,
                                    uint16_t dest,
                                    uint32_t conn_id)
{
    rina::AllocateFlowRequestResultEvent* afrrevent = nullptr;
    rina::IPCEvent* event = nullptr;
    rina::FlowSpecification qosspec;
    string saddr_str, source_str, daddr_str, dest_str;
    char strsp[8];
    char strdp[8];
    unsigned int seqnum;

    /* Concatenate 4-tuple into client_api */
    saddr_str  = uint32_to_string(saddr);
    daddr_str  = uint32_to_string(daddr);
    sprintf(strsp, "%u", source);
    source_str = string(strsp);
    sprintf(strdp, "%u", dest);
    dest_str   = string(strdp);

    std::stringstream tuple;
    tuple<<saddr_str<<"-"<<source_str<<"-"<<daddr_str<<"-"<<dest_str;
    client_api = tuple.str();

    qosspec.orderedDelivery = true;
    qosspec.msg_boundaries  = false;
    qosspec.maxAllowableGap = 0; 		      /* 0 for reliable flow */

    if (dif_name != string()) {
        seqnum = ipcManager->requestFlowAllocationInDIF(
                ApplicationProcessNamingInformation(client_apn, client_api),
                ApplicationProcessNamingInformation(server_apn, server_api),
                ApplicationProcessNamingInformation(dif_name, string()),
                qosspec);
    } else {
        seqnum = ipcManager->requestFlowAllocation(
                ApplicationProcessNamingInformation(client_apn, client_api),
                ApplicationProcessNamingInformation(server_apn, server_api),
                qosspec);
    }

    while (running) {
        event = nullptr;
        event = ipcEventProducer->eventWait();
        if (event && event->eventType == ALLOCATE_FLOW_REQUEST_RESULT_EVENT
                && event->sequenceNumber == seqnum) {
            break;
        } else {
            insertEvent(event->sequenceNumber, event);

            while (!(event = (IPCEvent *)findEvent(seqnum)) && running) {
                {
                    std::unique_lock<std::mutex> lck(event_lock);
                    cv.wait(lck);
                }
                continue;
            }
            eraseEvent(seqnum);
            break;
        }
    }
    if (!running)
        return -1;

    afrrevent = dynamic_cast<AllocateFlowRequestResultEvent*>(event);


    rina::FlowInformation flow =
        ipcManager->commitPendingFlow(afrrevent->sequenceNumber,
                                      afrrevent->portId,
                                      afrrevent->difName);
    if (flow.portId < 0) {
        LOG_ERR("thread %lu failed to allocate Fd{%u, ?}", pthread_self(),
                                                           conn_id);
        return -1;
    }

    LOG_DBG("thread %lu allocated Fd{%u, %d}", pthread_self(), conn_id,
                                               flow.portId);
    return flow.portId;
}

void FlowAllocatorClient::destroyFlow(uint32_t _conn_id)
{
    int port_id = findFd(_conn_id);

    if (port_id > 0) {
        ipcManager->deallocate_flow(port_id);
        eraseFd(_conn_id);
    }
}

FlowAllocatorClient::~FlowAllocatorClient()
{
    /* join threads */
    for (auto& th : threads)
        th.join();

    if (nl_sock)
        close(nl_sock);
}
