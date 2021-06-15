/*
 *  pep-dna/pepdna/kmodule/connection.h: PEP-DNA connection instance header
 *
 *  Copyright (C) 2020  Kristjon Ciko <kristjoc@ifi.uio.no>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _PEPDNA_CONNECTION_H
#define _PEPDNA_CONNECTION_H

#include "server.h"

#include <linux/kref.h>
#include <linux/netfilter.h>

/* timeout for RINA flow allocation in msec */
#define FLOW_ALLOC_TIMEOUT 7000
/* timeout for TCP connection in msec */
#define TCP_ACCEPT_TIMEOUT 7000

#define pepdna_hash(T, K) hash_min(K, HASH_BITS(T))

struct ipcp_flow;
extern struct pepdna_server *pepdna_srv;


/**
 * struct syn_tuple - 4-tuple of syn packet
 * @saddr   - source IP address
 * @ source - source TCP port
 * @daddr   - destination IP address
 * @dest    - destination TCP port
 */
struct syn_tuple {
    __be32 saddr;
    __be16 source;
    __be32 daddr;
    __be16 dest;
};

/**
 * struct pepdna_con - KPROXY connection struct
 * @kref:      reference counter to connection object
 * @server:    pointer to connected KPROXY server
 * @tcfa_work: TCP connect/RINA Flow Allocation after accept work item
 * @l2r_work:  left2right work item
 * @r2l_work:  right2left work item
 * @hlist:     node member in hash table
 * @flow:      RINA flow
 * @port_id:   port id of the flow
 * @lsock:     left TCP socket
 * @rsock:     right TCP socket
 * @lflag:     indicates left connection state
 * @rflag:     indicates left connection state
 * @hash_conn_id: 32-bit hash connection identifier
 * @tuple:     connection tuple
 * @skb:       initial SYN sk_buff
 */
struct pepdna_con {
    struct kref kref;
    struct pepdna_server *server;
    struct work_struct tcfa_work;
    struct work_struct l2r_work;
    struct work_struct r2l_work;
    struct hlist_node hlist;
#ifdef CONFIG_PEPDNA_RINA
    struct ipcp_flow *flow;
    atomic_t port_id;
#endif
    struct socket *lsock;
    struct socket *rsock;
    atomic_t lflag;
    atomic_t rflag;
    __u32 hash_conn_id;
    struct syn_tuple tuple;
    struct sk_buff *skb;
};

bool tcpcon_is_ready(struct pepdna_con *);
bool lconnected(struct pepdna_con *);
bool rconnected(struct pepdna_con *);
struct pepdna_con *pepdna_con_alloc(struct syn_tuple *, struct sk_buff *, int);
struct pepdna_con *pepdna_con_find(uint32_t);
void pepdna_con_get(struct pepdna_con *);
void pepdna_con_put(struct pepdna_con *);
void pepdna_con_close(struct pepdna_con *);

#endif /* _PEPDNA_CONNECTION_H */
