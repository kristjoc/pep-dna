/*
 *  rina/pepdna/connection.c: PEP-DNA connection instance
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

#include "connection.h"
#include "core.h"
#include "tcp.h"
#include "hash.h"

#ifdef CONFIG_PEPDNA_RINA
#include "rina.h"
#endif

#include <linux/sched.h>    /* included for wait_event_interruptible_timeout */
#include <net/sock.h>       /* included for struct sock */

/*
 * Get connection reference
 * ------------------------------------------------------------------------- */
void pepdna_con_get(struct pepdna_con *con)
{
        kref_get(&con->kref);
}

/*
 * Check if left TCP connection is active
 * ------------------------------------------------------------------------- */
bool lconnected(struct pepdna_con *con)
{
        return con && atomic_read(&con->lflag);
}

/*
 * Check if right  TCP connection is active
 * ------------------------------------------------------------------------- */
bool rconnected(struct pepdna_con *con)
{
        return con && atomic_read(&con->rflag);
}

/*
 * Check if TCP connection is established
 * ------------------------------------------------------------------------- */
bool tcpcon_is_ready(struct pepdna_con *con)
{
        struct sock* sk = NULL;

        if (con && con->rsock && con->rsock->sk)
                return sk->sk_state == TCP_ESTABLISHED;

        return false;
}

/*
 * Allocate a new pepdna_con and add it to the Hash Table
 * This function is called by the Hook func @'server.c'
 * ------------------------------------------------------------------------- */
struct pepdna_con *pepdna_con_alloc(struct syn_tuple *syn, struct sk_buff *skb,
                int port_id)
{
        struct pepdna_con *con = kzalloc(sizeof(struct pepdna_con), GFP_ATOMIC);
        if (!con)
                return NULL;

        kref_init(&con->kref);
        con->skb = (skb) ? skb_copy(skb, GFP_ATOMIC) : NULL;

        switch (pepdna_srv->mode) {
                case TCP2TCP:
                        INIT_WORK(&con->l2r_work, pepdna_con_li2ri_work);
                        INIT_WORK(&con->r2l_work, pepdna_con_ri2li_work);
                        INIT_WORK(&con->tcfa_work, pepdna_tcp_connect);
                        break;
#ifdef CONFIG_PEPDNA_RINA
                case TCP2RINA:
                        INIT_WORK(&con->l2r_work, pepdna_con_i2r_work);
                        INIT_WORK(&con->r2l_work, pepdna_con_r2i_work);
                        INIT_WORK(&con->tcfa_work, pepdna_flow_alloc);
                        break;
                case RINA2TCP:
                        INIT_WORK(&con->l2r_work, pepdna_con_i2r_work);
                        INIT_WORK(&con->r2l_work, pepdna_con_r2i_work);
                        INIT_WORK(&con->tcfa_work, pepdna_tcp_connect);
                        break;
                case RINA2RINA:
                        /* INIT_WORK(&con->l2r_work, pepdna_con_rl2rr_work); */
                        /* INIT_WORK(&con->r2l_work, pepdna_con_rr2rl_work); */
                        /* INIT_WORK(&con->tcfa_work, pepdna_flow_alloc); */
                        break;
#endif
#ifdef CONFIG_PEPDNA_CCN
                case TCP2CCN:
                        INIT_WORK(&con->l2r_work, pepdna_con_i2c_work);
                        INIT_WORK(&con->r2l_work, pepdna_con_ri2li_work);
                        INIT_WORK(&con->tcfa_work, pepdna_tcp_connect);
                        break;
                case CCN2TCP:
			/* TODO: Not supported yet! */
                        /* INIT_WORK(&con->l2r_work, pepdna_con_i2r_work); */
                        /* INIT_WORK(&con->r2l_work, pepdna_con_r2i_work); */
                        /* INIT_WORK(&con->tcfa_work, pepdna_tcp_connect); */
                        break;
                case CCN2CCN:
			/* TODO: Not supported yet*/
                        /* INIT_WORK(&con->l2r_work, pepdna_con_rl2rr_work); */
                        /* INIT_WORK(&con->r2l_work, pepdna_con_rr2rl_work); */
                        /* INIT_WORK(&con->tcfa_work, pepdna_flow_alloc); */
                        break;
#endif
                default:
                        pep_err("PEP-DNA mode undefined");
                        return NULL;
        }

        con->hash_conn_id = pepdna_hash32_rjenkins1_2(syn->saddr, syn->source);
#ifdef CONFIG_PEPDNA_RINA
        atomic_set(&con->port_id, port_id);
        con->flow = NULL;
#endif
        con->server = pepdna_srv;
        con->server->idr_in_use++;
        con->lsock  = NULL;
        con->rsock  = NULL;

        con->tuple.saddr  = syn->saddr;
        con->tuple.source = syn->source;
        con->tuple.daddr  = syn->daddr;
        con->tuple.dest   = syn->dest;

        INIT_HLIST_NODE(&con->hlist);
        hash_add(pepdna_srv->htable, &con->hlist, con->hash_conn_id);

        if (!queue_work(con->server->tcfa_wq, &con->tcfa_work)) {
                pep_err("tcfa_work was already on a queue_work");
                pepdna_con_put(con);
        }

        return con;
}

/*
 * Find connection in Hash Table
 * Called by: pepdna_tcp_accept() @'tcp_listen.c'
 *            nl_r2i_callback() @'server.c'
 * ------------------------------------------------------------------------- */
struct pepdna_con *pepdna_con_find(uint32_t key)
{
        struct pepdna_con* con   = NULL;
        struct pepdna_con* found = NULL;
        struct hlist_head *head  = NULL;
        struct hlist_node *next;

        rcu_read_lock();
        head = &pepdna_srv->htable[pepdna_hash(pepdna_srv->htable, key)];
        hlist_for_each_entry_safe(con, next, head, hlist) {
                if (con->hash_conn_id == key) {
                        found = con;
                        break;
                }
        }
        rcu_read_unlock();

        return found;
}

/*
 * Release connection after pepdna_con_put(con) is called
 * ------------------------------------------------------------------------- */
static void pepdna_con_kref_release(struct kref *kref)
{
        struct pepdna_con *con = container_of(kref, struct pepdna_con, kref);

        pep_debug("Cleaning up con. with hash_id %u", con->hash_conn_id);
        if (con->lsock) {
                sock_release(con->lsock);
                con->lsock = NULL;
        }
        if (con->rsock) {
                sock_release(con->rsock);
                con->rsock = NULL;
        }

        rcu_read_lock();
        hlist_del(&con->hlist);
        kfree(con); con = NULL;
        pepdna_srv->idr_in_use--;
        rcu_read_unlock();
}

/*
 * Destroy connection after pepdna_con_close(con)
 * ------------------------------------------------------------------------- */
void pepdna_con_put(struct pepdna_con *con)
{
        if (con)
                kref_put(&con->kref, pepdna_con_kref_release);
        else
                pep_err("FIX this immediately!!!");
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

        if (!con)
                return;

        lsk = (con->lsock) ? con->lsock->sk : NULL;
        if (!lsk)
                goto err;

        lconnected = atomic_read(&con->lflag);
        rconnected = atomic_read(&con->rflag);

        /* Close Left side */
        write_lock_bh(&lsk->sk_callback_lock);
        atomic_set(&con->lflag, 0);
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

                write_lock_bh(&rsk->sk_callback_lock);
                atomic_set(&con->rflag, 0);
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
                        pep_debug("r2l_work cancelled");
                }
#endif
        }
err:
        pepdna_con_put(con);
}
