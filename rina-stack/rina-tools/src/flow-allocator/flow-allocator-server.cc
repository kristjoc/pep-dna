/*
 * Server-Flow-Allocator
 *
 * Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
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

#include <algorithm>
#include <iterator>
#include <vector>
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

#define RINA_PREFIX "fallocator.server"
#include <librina/ipc-api.h>
#include <librina/logs.h>

#include "flow-allocator-server.h"
#include "common.h"

using namespace std;
using namespace rina;

FlowAllocatorServer::FlowAllocatorServer(const std::list<std::string>& _dif_names,
        const string& _server_apn,
        const string& _server_api,
        bool _app_reg) :
    Application(_dif_names, _server_apn, _server_api),
    dif_name(_dif_names.front()),
    server_apn(_server_apn),
    server_api(_server_api),
    app_reg(_app_reg),
    nr(0)
{
    /* Do nothing inside constructor */
}

void FlowAllocatorServer::insertFds(int _port_id, uint32_t _conn_id)
{
    std::lock_guard<std::mutex> guard(fds_lock);

    if (fds_map.size() > MAX_CONNS) {
        LOG_ERR("Max. sessions reached");
        return;
    }

    fds_map.insert({_port_id,_conn_id});

    LOG_DBG("Fds{%d, %u} added", _port_id, _conn_id);
}

void FlowAllocatorServer::insertEvents(int _port_id, void *_event)
{
    std::unique_lock<std::mutex> lck(events_lock);

    IPCEvent *evt = (IPCEvent *)_event;
    events_map.insert({_port_id, evt});

    LOG_DBG("Events_Map{%d, %p} added by thread %lu", _port_id, evt,
                                                      pthread_self());
}

int FlowAllocatorServer::findFds(int _port_id)
{
    std::unordered_map<int,uint32_t>::const_iterator it = fds_map.find(_port_id);

    if (it == fds_map.end()) {
        LOG_ERR("Fds{%d, ?} not found", _port_id);
        return -1;
    }

    return it->first;
}

void* FlowAllocatorServer::findEvents(int _port_id)
{
    std::unordered_map<int,void*>::const_iterator it = events_map.find(_port_id);

    if (it == events_map.end()) {
        LOG_ERR("Events_Map{%d, ?} not found", _port_id);
        return NULL;
    } else {
        LOG_DBG("Events_Map{%d, ?} found by thread %lu", _port_id, pthread_self());
        return (IPCEvent *)(it->second);
    }
}

void FlowAllocatorServer::eraseFds(int _port_id)
{
    std::lock_guard<std::mutex> guard(fds_lock);

    if (findFds(_port_id) > 0)
        fds_map.erase(_port_id);
    else
        LOG_ERR("Fds{%d, 0} to be deleted not found", _port_id);

    return;
}

void FlowAllocatorServer::eraseEvents(int _port_id)
{
    std::unique_lock<std::mutex> lck(events_lock);

    if (findEvents(_port_id)) {
        events_map.erase(_port_id);
        LOG_DBG("Events_Map{%d, ?} deleted", _port_id);
    } else
        LOG_ERR("Events_Map{%d, ?} to be deleted not found", _port_id);
}

void FlowAllocatorServer::ev_thread_fn(void *)
{
    rina::FlowInformation flow;
    int rc = 0;

    while (running) {
        rina::IPCEvent* event = ipcEventProducer->eventWait();

        if (!event)
            return;

        switch (event->eventType) {
        case REGISTER_APPLICATION_RESPONSE_EVENT:
            ipcManager->commitPendingRegistration(event->sequenceNumber,
                    dynamic_cast<RegisterApplicationResponseEvent*>(event)->DIFName);
            break;

        case UNREGISTER_APPLICATION_RESPONSE_EVENT:
            ipcManager->appUnregistrationResult(event->sequenceNumber,
                    dynamic_cast<UnregisterApplicationResponseEvent*>(event)->result == 0);
            break;

        case FLOW_ALLOCATION_REQUESTED_EVENT:
            {
                struct nl_msg nlmsg = {0};
                std::vector<std::string>tokens;
                std::string s;

                rina::FlowRequestEvent *flowRequestEvent = dynamic_cast<rina::FlowRequestEvent*>(event);

                std::istringstream iss(flowRequestEvent->remoteApplicationName.processInstance.c_str());

                while (std::getline(iss, s, '-'))
                    tokens.push_back(s);

                memset(&nlmsg, 0, sizeof(struct nl_msg));
                nlmsg.saddr  = cstring_to_uint32(tokens[0].c_str());
                nlmsg.source = cstring_to_uint16(tokens[1].c_str());
                nlmsg.daddr  = cstring_to_uint32(tokens[2].c_str());
                nlmsg.dest   = cstring_to_uint16(tokens[3].c_str());
                nlmsg.hash_conn_id = 0;
                nlmsg.port_id = flowRequestEvent->portId;
                nlmsg.alloc   = 1;

                rc = netlink_send_data(nl_sock, &nlmsg);
                if (rc < 0) {
                    LOG_ERR("Couldn't ask PEPDNA to initiate TCP connect()");
                    return;
                }
                /* Insert the flowRequestEvent to Events_Map and delete it only
                 * when flowResponse is sent (after the right TCP connection is
                 * established
                 */
                insertEvents(flowRequestEvent->portId, event);

                break;
            }

        case FLOW_DEALLOCATED_EVENT:
            {
                struct nl_msg nlmsg = {0};
                /* TODO This never happens */
                int port_id = dynamic_cast<FlowDeallocatedEvent*>(event)->portId;
                ipcManager->flowDeallocated(port_id);
                LOG_DBG("Flow torn down remotely [port-id = %d]", port_id);
                nlmsg.hash_conn_id = 0;
                nlmsg.port_id = port_id;

                if (netlink_send_data(nl_sock, &nlmsg) < 0) {
                    LOG_ERR("ERROR | netlink_sendmsg");
                    return;
                }

                break;
            }

        default:
            LOG_DBG("FlowAllocatorServer got new event of type %d", event->eventType);
            break;
        }
    }
    LOG_DBG("thread %lu terminated successfully", pthread_self());
}

void FlowAllocatorServer::run(bool blocking)
{
    rina::IPCEvent *event = nullptr;
    rina::FlowInformation flow;
    struct nl_msg *data  = nullptr;
    struct nlmsghdr *nlh = nullptr;
    struct msghdr msg;
    struct iovec iov;
    int rc = 0;

    if (app_reg) {
        applicationRegister();
    }

    nl_sock = netlink_init();
    if (nl_sock < 0) {
        LOG_ERR("netlink_init");
        return;
    }

    /* 2. Launch events thread */
    ev_thread = std::thread(&FlowAllocatorServer::ev_thread_fn, this, nullptr);

    /* 3. Prepare to recv data from Netlink socket */
    nlh = (struct nlmsghdr *)calloc(1, NLMSG_SPACE(NETLINK_MSS));
    if (!nlh) {
        LOG_ERR("calloc nlh");
        goto out;
    }

    while (running) {
        memset(nlh, 0, NLMSG_SPACE(NETLINK_MSS));
        memset(&iov, 0, sizeof(struct iovec));
        iov.iov_base = (void *)nlh;
        iov.iov_len = NLMSG_SPACE(NETLINK_MSS);

        memset(&msg, 0, sizeof(struct msghdr));
        msg.msg_iov    = &iov;
        msg.msg_iovlen = 1;

        rc = recvmsg(nl_sock, &msg, 0);
        if (rc > 0) {
            data = (struct nl_msg *)NLMSG_DATA(nlh);
            if (!data->alloc) { /* Right TCP connections will close */
                destroyFlow(data->port_id);
            } else if (data->alloc) { /* Right TCP connection is established */
                event = (IPCEvent *)findEvents(data->port_id);
                if (event) {
                    LOG_INFO("Sending FLOW_ALLOCATE_RESPONSE");
                    flow = ipcManager->allocateFlowResponse(
                        *dynamic_cast<rina::FlowRequestEvent*>(event), 0, true, blocking);
                    data->alloc = 0;
                    rc = netlink_send_data(nl_sock, data);
                    if (rc < 0) {
                        LOG_ERR("Couldn't confirm to PEPDNA that flow is allocated");
                        goto out;
                    }
                    eraseEvents(data->port_id);
                }
            }
        } else if (rc < 0) {
            if (errno == EAGAIN)
                continue;
            LOG_ERR("ERROR %d | recvmsg", errno);
            goto out;
        } else { /* rc = 0 */
            LOG_ERR("ERROR | EOF on netlink socket");
            goto out;
        }
    }
out:
    free(nlh); nlh = nullptr;
    if (nl_sock) {
        close(nl_sock); nl_sock = 0;
    }
}

void FlowAllocatorServer::destroyFlow(int _port_id)
{
    LOG_DBG("Deallocating flow with port-id %d", _port_id);
    ipcManager->deallocate_flow(_port_id);
    eraseFds(_port_id);
}

FlowAllocatorServer::~FlowAllocatorServer()
{
    ev_thread.join();

    if (nl_sock)
        close(nl_sock);
    nl_sock = 0;
}
