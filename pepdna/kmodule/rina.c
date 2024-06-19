/*
 *	pep-dna/kmodule/rina.c: PEP-DNA RINA support
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

#ifdef CONFIG_PEPDNA_RINA
#include "rina.h"
#include "core.h"
#include "connection.h"
#include "netlink.h"
#include "tcp_utils.h"
#include "hash.h"

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)
#include <linux/signal.h>
#else
#include <linux/sched/signal.h>
#endif

#ifdef CONFIG_PEPDNA_LOCAL_SENDER
#include <net/ip.h>
#endif

static int pepdna_con_i2rina_fwd(struct pepdna_con *);
static int pepdna_con_rina2i_fwd(struct pepdna_con *);


/*
 * Check if RINA flow is not destroyed
 * ------------------------------------------------------------------------- */
bool flow_is_ok(struct ipcp_flow *flow)
{
	if (flow && flow->wqs &&
	    (flow->state == PORT_STATE_ALLOCATED || flow->state == PORT_STATE_DISABLED))
		return true;

	return false;
}

/*
 * Check if flow is readable
 * ------------------------------------------------------------------------- */
bool queue_is_ready(struct ipcp_flow *flow)
{
	if (!flow_is_ok(flow) || !rfifo_is_empty(flow->sdu_ready))
		return true;

	return false;
}

/*
 * Wait for RINA flow to become readable
 * ------------------------------------------------------------------------- */
long pepdna_wait_for_sdu(struct ipcp_flow *flow)
{
	DEFINE_WAIT(wq_entry);
	signed long timeo = (signed long)usecs_to_jiffies(FLOW_POLL_TIMEOUT);

	for (;;) {
		if (flow_is_ok(flow)) {
			prepare_to_wait(&flow->wqs->read_wqueue, &wq_entry,
					TASK_INTERRUPTIBLE);
		} else {
			timeo = -ESHUTDOWN;
			return timeo;
		}

		if (timeo && rfifo_is_empty(flow->sdu_ready)) {
			timeo = schedule_timeout(timeo);
		}
		if (!timeo || queue_is_ready(flow))
		break;

		if (signal_pending(current)) {
			timeo = -ERESTARTSYS;
			break;
		}
	}

	if (flow_is_ok(flow)) {
		finish_wait(&flow->wqs->read_wqueue, &wq_entry);
	} else {
		timeo = -ESHUTDOWN;
		__set_current_state(TASK_RUNNING);
	}

	return timeo;
}

/*
 * Send DUs over a RINA flow
 * ------------------------------------------------------------------------- */
static int pepdna_flow_write(struct ipcp_flow *flow, int pid, unsigned char *buf,
			     size_t len)
{
	struct ipcp_instance *ipcp = NULL;
	struct du *du		   = NULL;
	size_t left		   = len;
	size_t max_du_size	   = 0;
	size_t copylen		   = 0;
	size_t sent		   = 0;
	int rc			   = 0;

	if (!flow) {
		pep_err("No flow bound to port_id %d", pid);
		return -EBADF;
	}

	if (flow->state < 0) {
		pep_err("Flow with port_id %d is already deallocated", pid);
		return -ESHUTDOWN;
	}

	ipcp = flow->ipc_process;

	max_du_size = ipcp->ops->max_sdu_size(ipcp->data);

	while (left) {
		copylen = min(left, max_du_size);
		du = du_create(copylen);
		if (!du) {
			rc = -ENOMEM;
			goto out;
		}

		memcpy(du_buffer(du), buf + sent, copylen);

		if (ipcp->ops->du_write(ipcp->data, pid, du, false)) {
			pep_err("Couldn't write SDU to port_id %d", pid);
			rc = -EIO;
			goto out;
		}

		left -= copylen;
		sent += copylen;
	}
out:
		return sent ? sent : rc;
}


void pepdna_rina_flow_alloc(struct work_struct *work)
{
	struct pepdna_con *con = container_of(work, struct pepdna_con,
					      tcfa_work);
	int rc = 0;

	/* Asking fallocator.client to initiate (1) a RINA flow allocation */
	rc = pepdna_nl_sendmsg(con->tuple.saddr, con->tuple.source,
			       con->tuple.daddr, con->tuple.dest,
			       con->id, atomic_read(&con->port_id), 1);
	if (rc < 0) {
		pep_err("Couldn't notify fallocator to allocate a flow");
		pepdna_con_close(con);
	}
}

/*
 * Check if flow has already a valid port-id and a !NULL flow
 * ------------------------------------------------------------------------- */
bool flow_is_ready(struct pepdna_con *con)
{
	struct ipcp_flow *flow = NULL;

	flow = kfa_flow_find_by_pid(kipcm_kfa(default_kipcm),
				    atomic_read(&con->port_id));
	if (flow) {
		pep_dbg("Flow with port_id %d is now ready",
			  atomic_read(&con->port_id));
		con->flow = flow;
		con->flow->state = PORT_STATE_ALLOCATED;
	} else {
		pep_dbg("Flow with port_id %d is not ready yet",
			  atomic_read(&con->port_id));
	}

	return flow && atomic_read(&con->port_id);
}

/*
 * Allocate wqs for the flow
 * ------------------------------------------------------------------------- */
static int pepdna_flow_set_iowqs(struct ipcp_flow *flow)
{
	struct iowaitqs *wqs = rkzalloc(sizeof(struct iowaitqs), GFP_KERNEL);
	if (!wqs)
		return -ENOMEM;

	init_waitqueue_head(&wqs->read_wqueue);
	init_waitqueue_head(&wqs->write_wqueue);

	flow->wqs = wqs;

	return 0;
}

/*
 * Forward data from RINA flow to TCP socket
 * ------------------------------------------------------------------------- */
int pepdna_con_rina2i_fwd(struct pepdna_con *con)
{
	struct kfa *kfa	     = kipcm_kfa(default_kipcm);
	struct socket *lsock = con->lsock;
	struct du *du        = NULL;
	int port_id	     = atomic_read(&con->port_id);
	bool blocking	     = false; /* Don't block while reading from the flow */
	signed long timeo    = 0;
	int read = 0, sent   = 0;

	IRQ_BARRIER;

	while (rconnected(con)) {
		timeo = pepdna_wait_for_sdu(con->flow);
		if (timeo > 0)
			break;
		if (timeo == -ERESTARTSYS || timeo == -ESHUTDOWN || timeo == -EINTR)
			return -1;
	}

	read = kfa_flow_du_read(kfa, port_id, &du, MAX_SDU_SIZE, blocking);
	if (read <= 0) {
		pep_dbg("kfa_flow_du_read %d", read);
		return read;
	}

	if (!is_du_ok(du))
		return -EIO;

	sent = pepdna_sock_write(lsock, du_buffer(du), read);
	if (sent < 0) {
		pep_dbg("error forwarding from flow to socket");
		read = -1;
	}

	du_destroy(du);
	return read;
}

/*
 * Forward data from TCP socket to RINA flow
 * ------------------------------------------------------------------------- */
int pepdna_con_i2rina_fwd(struct pepdna_con *con)
{
	struct socket *lsock   = con->lsock;
	struct ipcp_flow *flow = con->flow;
	unsigned char *buffer  = NULL;
	int port_id = atomic_read(&con->port_id);
	int read = 0, sent = 0;

	struct msghdr msg = {
		.msg_flags = MSG_DONTWAIT,
	};
	struct kvec vec;

	/* allocate buffer memory */
	buffer = kzalloc(MAX_BUF_SIZE, GFP_KERNEL);
	if (!buffer) {
		pep_err("Failed to alloc buffer");
		return -ENOMEM;
	}
	vec.iov_base = buffer;
	vec.iov_len  = MAX_BUF_SIZE;
	read = kernel_recvmsg(lsock, &msg, &vec, 1, vec.iov_len, MSG_DONTWAIT);
	if (likely(read > 0)) {
		sent = pepdna_flow_write(flow, port_id, buffer, read);
		if (sent < 0) {
			pep_err("error forwarding to flow %d", port_id);
			kfree(buffer);
			return -1;
		}
	} else {
		if (read == -EAGAIN || read == -EWOULDBLOCK)
		pep_dbg("kernel_recvmsg() returned %d", read);
	}

	kfree(buffer);
	return read;
}

/*
 * Netlink callback for RINA2TCP mode
 * ------------------------------------------------------------------------- */
void nl_r2i_callback(struct nl_msg *nlmsg)
{
	struct pepdna_con *con = NULL;
	struct syn_tuple *syn  = NULL;
	uint32_t hash_id;

	if (nlmsg->alloc) {
		syn = (struct syn_tuple *)kzalloc(sizeof(struct syn_tuple),
						  GFP_ATOMIC);
		if (IS_ERR(syn)) {
			pep_err("kzalloc");
			return;
		}
		syn->saddr  = cpu_to_be32(nlmsg->saddr);
		syn->source = cpu_to_be16(nlmsg->source);
		syn->daddr  = cpu_to_be32(nlmsg->daddr);
		syn->dest   = cpu_to_be16(nlmsg->dest);

		hash_id = pepdna_hash32_rjenkins1_2(syn->saddr, syn->source);
		con = pepdna_con_alloc(syn, NULL, hash_id, 0ull, nlmsg->port_id);
		if (!con)
			pep_err("pepdna_con_alloc");

		kfree(syn);
	} else {
		con = pepdna_con_find(nlmsg->id);
		if (!con) {
			pep_err("Connection was removed from Hash Table");
			return;
		}
		if (flow_is_ready(con)) {
			/* At this point, right TCP connection is established
			 * and RINA flow is allocated. Queue r2i_work now!
			 */

			if (!con->flow->wqs)
				pepdna_flow_set_iowqs(con->flow);

			atomic_set(&con->rflag, 1);
			atomic_set(&con->lflag, 1);

			pepdna_con_get(con);
			if (!queue_work(con->server->r2l_wq, &con->r2l_work)) {
				pep_err("r2i_work was already on a queue");
				pepdna_con_put(con);
				return;
			}
			/* Wake up 'left' socket */
			con->lsock->sk->sk_data_ready(con->lsock->sk);
		}
	}
}

/*
 * Netlink callback for TCP2RINA mode
 * ------------------------------------------------------------------------- */
void nl_i2r_callback(struct nl_msg *nlmsg)
{
	struct pepdna_con *con = NULL;

	con = pepdna_con_find(nlmsg->id);
	if (!con) {
		pep_err("Connection not found in Hash table");
		return;
	}
	atomic_set(&con->port_id, nlmsg->port_id);

	if (flow_is_ready(con)) {
		atomic_set(&con->rflag, 1);

		if (!con->flow->wqs)
			pepdna_flow_set_iowqs(con->flow);

		/* At this point, RINA flow is allocated. Reinject SYN in back
		 * in the stack so that the left TCP connection can be
		 * established There is no need to set callbacks here for the
		 * left socket as pepdna_tcp_accept() will take care of it.
		 */
		pep_dbg("Reinjecting initial SYN packet");
#ifndef CONFIG_PEPDNA_LOCAL_SENDER
		netif_receive_skb(con->skb);
#else
		struct net *net = sock_net(con->server->listener->sk);
		ip_local_out(net, con->server->listener->sk, con->skb);
#endif
	}
}

/*
 * TCP2RINA
 * Forward traffic from INTERNET to RINA
 * ------------------------------------------------------------------------- */
void pepdna_con_i2r_work(struct work_struct *work)
{
	struct pepdna_con *con = container_of(work, struct pepdna_con, l2r_work);
	int rc = 0;

	while (lconnected(con)) {
		if ((rc = pepdna_con_i2rina_fwd(con)) <= 0) {
			if (rc == -EAGAIN) //FIXME Handle -EAGAIN flood
				break;

			/* Tell fallocator in userspace to dealloc. the flow */
			rc = pepdna_nl_sendmsg(0, 0, 0, 0, con->id,
					       atomic_read(&con->port_id), 0);
			if (rc < 0)
				pep_err("Couldn't initiate flow dealloc.");
			pepdna_con_close(con);
		}
	}
	pepdna_con_put(con);
}

/*
 * RINA2TCP
 * Forward traffic from RINA to INTERNET
 * ------------------------------------------------------------------------- */
void pepdna_con_r2i_work(struct work_struct *work)
{
	struct pepdna_con *con = container_of(work, struct pepdna_con, r2l_work);
	int rc = 0;

	while (rconnected(con)) {
		if ((rc = pepdna_con_rina2i_fwd(con)) <= 0) {
			if (rc == -EAGAIN) {
				pep_dbg("Flow is not readable %d", rc);
				cond_resched();
			} else {
				atomic_set(&con->rflag, 0);
				pepdna_con_close(con);
			}
		}
	}
	pepdna_con_put(con);
}
#endif
