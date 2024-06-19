/*
 *	pep-dna/kmodule/connection.c: PEP-DNA connection instance
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

#include "connection.h"
#include "core.h"
#include "tcp.h"
#include "hash.h"

#ifdef CONFIG_PEPDNA_RINA
#include "rina.h"
#endif

#ifdef CONFIG_PEPDNA_MINIP
#include "minip.h"
#endif

#ifdef CONFIG_PEPDNA_CCN
#include "ccn.h"
#endif

#include <linux/sched.h>	/* included for wait_event_interruptible_timeout */
#include <net/sock.h>		/* included for struct sock */


/*
 * Check if left connection is active
 * ------------------------------------------------------------------------- */
bool lconnected(struct pepdna_con *con)
{
	return con && atomic_read(&con->lflag);
}

/*
 * Check if right connection is active
 * ------------------------------------------------------------------------- */
bool rconnected(struct pepdna_con *con)
{
	return con && atomic_read(&con->rflag);
}

/*
 * Allocate a new pepdna_con and add it to the Hash Table
 * This function is called by the Hook func @'server.c'
 * ------------------------------------------------------------------------- */
struct pepdna_con *pepdna_con_alloc(struct syn_tuple *syn, struct sk_buff *skb,
				    uint32_t hash_id, uint64_t ts, int port_id)
{
	struct pepdna_con *con = kzalloc(sizeof(struct pepdna_con), GFP_ATOMIC);
	if (!con)
		return NULL;

	kref_init(&con->kref);
	con->skb = (skb) ? skb_copy(skb, GFP_ATOMIC) : NULL;

	switch (pepdna_srv->mode) {
	case TCP2TCP:
		INIT_WORK(&con->l2r_work, pepdna_con_l2r_fwd);
		INIT_WORK(&con->r2l_work, pepdna_con_r2l_fwd);
		INIT_WORK(&con->tcfa_work, pepdna_tcp_connect);
		break;
#ifdef CONFIG_PEPDNA_RINA
	case TCP2RINA:
		INIT_WORK(&con->l2r_work, pepdna_con_i2r_work);
		INIT_WORK(&con->r2l_work, pepdna_con_r2i_work);
		INIT_WORK(&con->tcfa_work, pepdna_rina_flow_alloc);
		break;
	case RINA2TCP:
		INIT_WORK(&con->l2r_work, pepdna_con_i2r_work);
		INIT_WORK(&con->r2l_work, pepdna_con_r2i_work);
		INIT_WORK(&con->tcfa_work, pepdna_tcp_connect);
		break;
	case RINA2RINA:
		/* INIT_WORK(&con->l2r_work, pepdna_con_rl2rr_work); */
		/* INIT_WORK(&con->r2l_work, pepdna_con_rr2rl_work); */
		/* INIT_WORK(&con->tcfa_work, pepdna_rina_flow_alloc); */
		break;
#endif
#ifdef CONFIG_PEPDNA_MINIP
	case TCP2MINIP:
		INIT_WORK(&con->l2r_work, pepdna_con_i2m_work);
		INIT_WORK(&con->r2l_work, pepdna_con_m2i_work);
		INIT_WORK(&con->tcfa_work, pepdna_minip_handshake);
		break;
	case MINIP2TCP:
		INIT_WORK(&con->l2r_work, pepdna_con_i2m_work);
		INIT_WORK(&con->r2l_work, pepdna_con_m2i_work);
		INIT_WORK(&con->tcfa_work, pepdna_tcp_connect); // FIXME
		break;
#endif
#ifdef CONFIG_PEPDNA_CCN
	case TCP2CCN:
		INIT_WORK(&con->l2r_work, pepdna_con_i2c_work);
		INIT_WORK(&con->r2l_work, pepdna_con_c2i_work);
		INIT_WORK(&con->tcfa_work, pepdna_udp_open);
		break;
	case CCN2TCP:
		/* TODO: Not supported yet! */
		/* INIT_WORK(&con->l2r_work, pepdna_con_c2i_work); */
		/* INIT_WORK(&con->r2l_work, pepdna_con_i2c_work); */
		break;
	case CCN2CCN:
		/* TODO: Not supported yet*/
		/* INIT_WORK(&con->l2r_work, pepdna_con_lc2rc_work); */
		/* INIT_WORK(&con->r2l_work, pepdna_con_rc2lc_work); */
		break;
#endif
	default:
		pep_err("pepdna mode undefined");
		return NULL;
	}

	con->id = hash_id;
	con->ts = ts;
#ifdef CONFIG_PEPDNA_RINA
	atomic_set(&con->port_id, port_id);
	con->flow = NULL;
#endif
#ifdef CONFIG_PEPDNA_MINIP
        con->next_seq = MINIP_FIRST_SEQ;
        atomic_set(&con->last_acked, MINIP_FIRST_SEQ);
        con->next_recv = MINIP_FIRST_SEQ;
	con->window = WINDOW_SIZE;

        /* Create the retransmission queue for MINIP flow control */
        con->rtxq = rtxq_create();
        if (!con->rtxq) {
                pep_err("Failed to create rtxq instance");
                kfree(con);
                return NULL;
        }
	atomic_set(&con->sending, 1);
	/* Initialize dup_acks counter to 0 */
        atomic_set(&con->dup_acks, 0);

        /* RTO initial value is 3 seconds.
	 * Details in Section 2.1 of RFC6298
	 */
	con->rto = 3000;
	con->srtt = 0;
	con->rttvar = 0;

	timer_setup(&con->timer, minip_sender_timeout, 0);
#endif
	con->server = pepdna_srv;
	atomic_inc(&con->server->conns);
	con->lsock	= NULL;
	con->rsock	= NULL;
	atomic_set(&con->lflag, 0);
	atomic_set(&con->rflag, 0);

	con->tuple.saddr  = syn->saddr;
	con->tuple.source = syn->source;
	con->tuple.daddr  = syn->daddr;
	con->tuple.dest	  = syn->dest;

	INIT_HLIST_NODE(&con->hlist);
	hash_add(pepdna_srv->htable, &con->hlist, con->id);

	if (!queue_work(con->server->tcfa_wq, &con->tcfa_work)) {
		pep_err("failed to queue tcfa_work");
		pepdna_con_put(con);
	}

	return con;
}

/*
 * Find connection in Hash Table
 * Called by: pepdna_tcp_accept() @'tcp_listen.c'
 *		  nl_r2i_callback() @'server.c'
 * ------------------------------------------------------------------------- */
struct pepdna_con *pepdna_con_find(uint32_t key)
{
	struct pepdna_con* con	 = NULL;
	struct pepdna_con* found = NULL;
	struct hlist_head *head	 = NULL;
	struct hlist_node *next;

	rcu_read_lock();
	head = &pepdna_srv->htable[pepdna_hash(pepdna_srv->htable, key)];
	hlist_for_each_entry_safe(con, next, head, hlist) {
		if (con->id == key) {
			found = con;
			break;
		}
	}
	rcu_read_unlock();

	return found;
}

/*
 * Close Connection => Flow
 * ------------------------------------------------------------------------- */
void pepdna_con_close(struct pepdna_con *con)
{
        struct sock *lsk = NULL;
        struct sock *rsk = NULL;
        bool lconnected  = false;
        bool rconnected  = false;
#ifdef CONFIG_PEPDNA_RINA
        struct ipcp_flow *flow = (con) ? con->flow : NULL;
#endif

        if (!con) {
		pep_dbg("Oops, con is being closed but is already NULL");
		return;
        }

        lsk = (con->lsock) ? con->lsock->sk : NULL;
        if (!lsk)
                goto err;

        lconnected = atomic_read(&con->lflag);
        rconnected = atomic_read(&con->rflag);

        /* Close Left side */
        atomic_set(&con->lflag, 0);
        write_lock_bh(&lsk->sk_callback_lock);
        if (lconnected)
                lsk->sk_user_data = NULL;
        write_unlock_bh(&lsk->sk_callback_lock);

        if (lconnected)
                kernel_sock_shutdown(con->lsock, SHUT_RDWR);

        /* Close Right side (might be TCP or RINA) */
        if (con->server->mode == TCP2TCP) {
                rsk = (con->rsock) ? con->rsock->sk : NULL;
                if (!rsk)
                        goto err;

		atomic_set(&con->rflag, 0);
                write_lock_bh(&rsk->sk_callback_lock);
                if (rconnected)
                        rsk->sk_user_data = NULL;
                write_unlock_bh(&rsk->sk_callback_lock);

                if (rconnected)
                        kernel_sock_shutdown(con->rsock, SHUT_RDWR);
        } else {
#ifdef CONFIG_PEPDNA_RINA
                atomic_set(&con->rflag, 0);
                if (rconnected) {
                        if (flow && flow->wqs)
                                wake_up_interruptible_all(&flow->wqs->read_wqueue);

                        if (con)
                                cancel_work_sync(&con->r2l_work);
                        pep_dbg("RINA r2l_work cancelled");
                }
#endif
#ifdef CONFIG_PEPDNA_MINIP
		del_timer(&con->timer);

                if (rconnected) {
			pep_dbg("Not ready to close the connection");
			return;
                } else {
			if (rtxq_destroy(con->rtxq)) {
				pep_err("failed to destroy MINIP rtxq queue");
			}
                        pep_dbg("destroyed MINIP rtxq queue");
                }
#endif
        }
err:
        pepdna_con_put(con);
}

/*
 * Release connection after pepdna_con_put(con) is called
 * ------------------------------------------------------------------------- */
static void pepdna_con_kref_release(struct kref *kref)
{
	struct pepdna_con *con = container_of(kref, struct pepdna_con, kref);
	if (!con)
		return;

	if (con->lsock) {
		sock_release(con->lsock);
		con->lsock = NULL;
	}
	if (con->rsock) {
		sock_release(con->rsock);
		con->rsock = NULL;
	}

	hlist_del(&con->hlist);
        kfree(con);
        con = NULL;

        pep_dbg("Freeing connection instance");
	atomic_dec(&pepdna_srv->conns);
}

/*
 * Release the reference of connection instance
 * ------------------------------------------------------------------------- */
void pepdna_con_put(struct pepdna_con *con)
{
	if (con && kref_read(&con->kref) > 0)
		kref_put(&con->kref, pepdna_con_kref_release);
}

/*
 * Get reference to connection instance
 * ------------------------------------------------------------------------- */
void pepdna_con_get(struct pepdna_con *con)
{
	kref_get(&con->kref);
}
