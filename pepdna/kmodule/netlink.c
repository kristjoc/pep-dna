/*
 *  pep-dna/kmodule/netlink.c: PEP-DNA Netlink code
 *
 *  Copyright (C) 2023	Kristjon Ciko <kristjoc@ifi.uio.no>
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


#include "netlink.h"
#include "core.h"	 /* pep_dbg */
#include "server.h"	 /* nl_callbacks and server_mode enum */
#ifdef CONFIG_PEPDNA_RINA
#include "rina.h"
#endif

#include <linux/workqueue.h>
#include <net/net_namespace.h>
#include <net/netlink.h>
#include <net/sock.h>

extern int mode; /* Declared in 'core.c' */

static enum server_mode pepdna_mode __read_mostly;
static struct sock *nl_sock = NULL;
static int nl_port_id	    = -1;

/*
 * Receive a message from a Netlink socket
 * ------------------------------------------------------------------------- */
static void pepdna_nl_recv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	struct nl_msg *data  = NULL;

	if (skb->len >= nlmsg_total_size(0)) {
		pep_dbg("receiving nl_msg from fallocator");

		nlh = nlmsg_hdr(skb);
		data = NULL;

		switch (pepdna_mode) {
#ifdef CONFIG_PEPDNA_RINA
		case TCP2RINA:
			if (nl_port_id == -1) {
				nl_port_id = nlh->nlmsg_pid;
			} else {
				data = (struct nl_msg *)NLMSG_DATA(nlh);
				nl_i2r_callback(data);
			}
			break;
		case RINA2TCP:
			if (nl_port_id == -1) {
				nl_port_id = nlh->nlmsg_pid;
			} else {
				data = (struct nl_msg *)NLMSG_DATA(nlh);
				nl_r2i_callback(data);
			}
			break;
#endif
		default:
			pep_dbg("netlink not needed in this mode");
			break;
		}
	}
}

/*
 * Send a message over a Netlink socket
 * ------------------------------------------------------------------------- */
int pepdna_nl_sendmsg(__be32 saddr, __be16 source, __be32 daddr, __be16 dest,
					  uint32_t id, int port_id, bool alloc)
{
	struct sk_buff *skb  = NULL;
	struct nlmsghdr *nlh = NULL;
	void *data	         = NULL;
	struct nl_msg nlmsg  = {0};
	int rc = 0, attempts = 0;

retry:
	pep_dbg("sending nl_msg(%d) to fallocator", alloc);
	skb = alloc_skb(NLMSG_SPACE(NETLINK_MSS), GFP_ATOMIC);
	if (!skb) {
		pep_err("alloc_skb");
		return -ENOMEM;
	}
	NETLINK_CB(skb).portid	  = 0;
	NETLINK_CB(skb).dst_group = 0;

	nlh = nlmsg_put(skb, 0, 0, 0, NETLINK_MSS, 0);
	if (!nlh) {
		kfree_skb(skb);
		return -1;
	}

	memset(&nlmsg, 0, sizeof(struct nl_msg));
	nlmsg.saddr  = be32_to_cpu(saddr);
	nlmsg.source = be16_to_cpu(source);
	nlmsg.daddr  = be32_to_cpu(daddr);
	nlmsg.dest   = be16_to_cpu(dest);
	nlmsg.id     = id;
	nlmsg.port_id = port_id;
	nlmsg.alloc = alloc;

	data = (void *) &nlmsg;
	memcpy(NLMSG_DATA(nlh), data, sizeof(struct nl_msg));

	rc = netlink_unicast(nl_sock, skb, nl_port_id, MSG_DONTWAIT);
	if (rc < 0) {
		if (rc == -ECONNREFUSED && attempts++ < NL_SEND_RETRIES) {
			pep_dbg("Retrying (%d) netlink_unicast", attempts);
			goto retry;
		}
		pep_err("netlink_unicast failed %d", rc);
	}
	return rc;
}

/*
 * init Netlink socket
 * ------------------------------------------------------------------------- */
int pepdna_netlink_init(void)
{
	struct netlink_kernel_cfg cfg = {0};

	cfg.input = pepdna_nl_recv_msg;
	pepdna_mode = mode;

	nl_sock = netlink_kernel_create(&init_net, NL_PEPDNA_PROTO, &cfg);
	if (!nl_sock) {
		pep_err("netlink_kernel_create");
		return -ENOMEM;
	}

	/* Set Netlink socket buffer size */
	nl_sock->sk_userlocks |= NL_SOCK_SNDBUF_LOCK;
	nl_sock->sk_sndbuf     = NL_SNDBUF_DEF;

	nl_sock->sk_userlocks |= NL_SOCK_RCVBUF_LOCK;
	nl_sock->sk_rcvbuf     = NL_RCVBUF_DEF;

	return 0;
}

/*
 * Release Netlink socket
 * ------------------------------------------------------------------------- */
void pepdna_netlink_stop(void)
{
	if (nl_sock) {
		netlink_kernel_release(nl_sock);
		nl_sock = NULL;
		pep_dbg("netlink socket released");
	}
}
