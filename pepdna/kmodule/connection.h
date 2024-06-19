/*
 *	pep-dna/kmodule/connection.h: PEP-DNA connection instance header
 *
 *	Copyright (C) 2023	Kristjon Ciko <kristjoc@ifi.uio.no>
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _PEPDNA_CONNECTION_H
#define _PEPDNA_CONNECTION_H

#include "server.h"

#include <linux/kref.h>
#include <linux/netfilter.h>

#ifdef CONFIG_PEPDNA_RINA
struct ipcp_flow;
/* timeout for RINA flow allocation in msec */
#define FLOW_ALLOC_TIMEOUT 3000
#endif
/* timeout for TCP connection in msec */
#define TCP_ACCEPT_TIMEOUT 3000

#define pepdna_hash(T, K) hash_min(K, HASH_BITS(T))

#ifdef CONFIG_PEPDNA_MINIP
struct rtxq;
#endif

extern struct pepdna_server *pepdna_srv;


/**
 * struct syn_tuple - 4-tuple of syn packet
 * @saddr	    - source IP address
 * @ source         - source TCP port
 * @daddr	    - destination IP address
 * @dest	    - destination TCP port
 */
struct syn_tuple {
	__be32 saddr;
	__be16 source;
	__be32 daddr;
	__be16 dest;
};

/**
 * struct pepdna_con - pepdna connection struct
 * @kref:	   reference counter to connection object
 * @server:	   pointer to connected KPROXY server
 * @tcfa_work:     TCP connect/RINA Flow Allocation after accept work item
 * @l2r_work:      left2right work item
 * @r2l_work:      right2left work item
 * @hlist:	   node member in hash table
 * @flow:	   RINA flow
 * @port_id:       port id of the flow
 * @rtxq:          MINIP retransmission queue
 * @lsock:	   left TCP socket
 * @rsock:	   right TCP socket
 * @lflag:	   indicates left connection state
 * @rflag:	   indicates left connection state
 * @id:            32-bit hash connection identifier
 * @ts:		   timestamp of the first incoming SYN
 * @tuple:	   connection tuple
 * @skb:	   initial SYN sk_buff
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
#ifdef CONFIG_PEPDNA_MINIP
	/* sender variables */
	/** @rtxq: retransmission queue pointer */
	struct rtxq *rtxq;
	/** @timer: timer for RTO */
	struct timer_list timer;
	/** @dup_acks: duplicate ACKs counter */
	atomic_t dup_acks;
	/** @sending: sending binary semaphore: 1 if sending, 0 is not */
	atomic_t sending;
	 /** @last_acked: last acked packet */
	atomic_t last_acked;
	/** @next_seq: next packet to be sent */
        u32 next_seq;
	/** @state: connection state */
        u8 state;
	/** @window: congestion window */
        u8 window;
	/* receiver variables */
	/** @next_recv: next in-order packet expected */
	u32 next_recv;
        	/** @rto: sender timeout in milliseconds */
	u32 rto;
	/** @srtt: smoothed RTT scaled by 2^3 */
	u32 srtt;
	/** @rttvar: RTT variation scaled by 2^2 */
	u32 rttvar;
#endif
	struct socket *lsock;
	struct socket *rsock;
	atomic_t lflag;
	atomic_t rflag;
	u32 id;
	u64 ts;
	struct syn_tuple tuple;
	struct sk_buff *skb;
};

bool lconnected(struct pepdna_con *);
bool rconnected(struct pepdna_con *);
struct pepdna_con *pepdna_con_find(u32);
struct pepdna_con *pepdna_con_alloc(struct syn_tuple *, struct sk_buff *, u32,
                                    u64, int);
void pepdna_con_get(struct pepdna_con *);
void pepdna_con_put(struct pepdna_con *);
void pepdna_con_close(struct pepdna_con *);

#endif /* _PEPDNA_CONNECTION_H */
