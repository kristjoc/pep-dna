/*
 *  rina/pepdna/server.c: PEP-DNA server infrastructure
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

#include "server.h"
#include "core.h"
#include "connection.h"
#include "tcp.h"
#include "tcp_utils.h"
#include "hash.h"
#include "netlink.h"

#ifdef CONFIG_PEPDNA_RINA
#include "rina.h"
#endif

#include <linux/kthread.h>
#include <linux/in.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/tcp.h>

/* External variables */
extern int pepdna_mode;
extern int port;

/* Global variables */
struct pepdna_server *pepdna_srv = NULL;

/* Static functions */
static unsigned int pepdna_pre_hook(void *, struct sk_buff *,
                const struct nf_hook_state *);
static int init_pepdna_server(struct pepdna_server *);
static int pepdna_i2i_start(struct pepdna_server *);
#ifdef CONFIG_PEPDNA_RINA
static int pepdna_r2r_start(struct pepdna_server *);
static int pepdna_r2i_start(struct pepdna_server *);
static int pepdna_i2r_start(struct pepdna_server *);
#endif
#ifdef CONFIG_PEPDNA_CCN
static int pepdna_i2c_start(struct pepdna_server *);
static int pepdna_c2i_start(struct pepdna_server *);
#endif

/*
 * Init workqueue_struct
 * This function is called by pepdna_from2to_start() @'server.c'
 * ------------------------------------------------------------------------- */
int pepdna_work_init(struct pepdna_server *srv)
{
        struct workqueue_struct *wq = NULL;
        int max_active = 0; /* default value - set 1 for ordered wq */

        wq = alloc_workqueue("l2r_wq", WQ_HIGHPRI|WQ_UNBOUND, max_active);
        if (!wq) {
                pep_err("Couldn't alloc pepdna l2r_fwd workqueue");
                goto err;
        }
        srv->l2r_wq = wq;

        wq = alloc_workqueue("r2l_wq", WQ_HIGHPRI|WQ_UNBOUND, max_active);
        if (!wq) {
                pep_err("Couldn't alloc pepdna r2l_fwd workqueue");
                goto free_r2l_wq;
        }
        srv->r2l_wq = wq;

        wq = alloc_workqueue("accept_wq", WQ_HIGHPRI|WQ_UNBOUND, max_active);
        if (!wq) {
                pep_err("Couldn't alloc pepdna_accept_workqueue");
                goto free_accept_wq;
        }
        srv->accept_wq = wq;

        if (srv->mode < 4) { /* First four enum PEPDNA modes */
                wq = alloc_workqueue("connect_alloc_wq", WQ_HIGHPRI|WQ_UNBOUND,
                                max_active);
                if (!wq) {
                        pep_err("Couldn't alloc pepdna tcp_connect/flow_alloc_workqueue");
                        goto free_tcfa_wq;
                }
                srv->tcfa_wq = wq;
        }

        return 0;
free_tcfa_wq:
        wq = srv->accept_wq;
        srv->accept_wq = NULL;
        destroy_workqueue(wq);
free_accept_wq:
        wq = srv->r2l_wq;
        srv->r2l_wq = NULL;
        destroy_workqueue(wq);
free_r2l_wq:
        wq = srv->l2r_wq;
        srv->l2r_wq = NULL;
        destroy_workqueue(wq);
err:
        return -ENOMEM;
}

/*
 * Stop workqueue_struct
 * ------------------------------------------------------------------------- */
void pepdna_work_stop(struct pepdna_server *srv)
{
        struct workqueue_struct *wq = NULL;

        if (srv->r2l_wq) {
                wq = srv->r2l_wq;
                flush_workqueue(wq);
                destroy_workqueue(wq);
                srv->r2l_wq = NULL;
        }
        if (srv->tcfa_wq) {
                wq = srv->tcfa_wq;
                flush_workqueue(wq);
                destroy_workqueue(wq);
                srv->tcfa_wq = NULL;
        }
        if (srv->accept_wq) {
                wq = srv->accept_wq;
                flush_workqueue(wq);
                destroy_workqueue(wq);
                srv->accept_wq = NULL;
        }
        if (srv->l2r_wq) {
                wq = srv->l2r_wq;
                flush_workqueue(wq);
                destroy_workqueue(wq);
                srv->l2r_wq = NULL;
        }
}

/*
 * Netlink callback for RINA2TCP mode
 * ------------------------------------------------------------------------- */
#ifdef CONFIG_PEPDNA_RINA
void nl_r2i_callback(struct nl_msg *nlmsg)
{
        struct pepdna_con *con = NULL;
        struct syn_tuple *syn  = NULL;

        pep_debug("r2_icallback is being called");
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

                con = pepdna_con_alloc(syn, NULL, nlmsg->port_id);
                if (!con)
                        pep_err("pepdna_con_alloc");

                kfree(syn);
                pep_debug("r2i_callback terminated");
        } else {
                con = pepdna_con_find(nlmsg->hash_conn_id);
                if (!con) {
                        pep_err("Connection was removed from Hash Table");
                        return;
                }
                if (flow_is_ready(con)) {
                        /* At this point, right TCP connection is established
                         * and RINA flow is allocated. Queue r2i_work now!
                         */
                        atomic_set(&con->rflag, 1);
                        atomic_set(&con->lflag, 1);
                        if (!queue_work(con->server->r2l_wq, &con->r2l_work)) {
                                pep_err("r2i_work was already on a queue");
                                pepdna_con_put(con);
                                return;
                        }
                        /* Wake up 'left' socket */
                        con->lsock->sk->sk_data_ready(con->lsock->sk);
                }
        }
        pep_debug("r2i_callback terminated");
}

/*
 * Netlink callback for TCP2RINA mode
 * ------------------------------------------------------------------------- */
void nl_i2r_callback(struct nl_msg *nlmsg)
{
        struct pepdna_con *con = pepdna_con_find(nlmsg->hash_conn_id);
        if (!con) {
                pep_err("Connection not found in Hash Table");
                return;
        }
        atomic_set(&con->port_id, nlmsg->port_id);

        /* At this point, RINA flow is allocated and con->flow is set by
         * flow_is_ready() function. Reinject SYN back to the stack so that
         * the left TCP connection can be established. There is no need to set
         * callbacks here for the left socket as pepdna_tcp_accept() will take
         * care of it.
         */

        if (flow_is_ready(con)) {
                atomic_set(&con->rflag, 1);
                netif_receive_skb(con->skb);
        }

        pep_debug("i2r_callback terminated");
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
                if ((rc = pepdna_con_i2r_fwd(con)) <= 0) {
                        if (rc == -EAGAIN) //FIXME Handle -EAGAIN flood
                                break;

                        /* Tell fallocator in userspace to dealloc. the flow */
                        rc = pepdna_nl_sendmsg(0, 0, 0, 0, con->hash_conn_id,
                                        atomic_read(&con->port_id), 0);
                        if (rc < 0)
                                pep_err("Couldn't notify fallocator to dealloc"
                                                " the flow");
                        pepdna_con_close(con);
                        break;
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

        pepdna_con_get(con);
        while (rconnected(con)) {
                if ((rc = pepdna_con_r2i_fwd(con)) <= 0) {
                        if (rc == -EAGAIN) {
                                pep_debug("Flow is not readable %d", rc);
                                cond_resched();
                        } else {
                                atomic_set(&con->rflag, 0);
                                pepdna_con_close(con);
                                break;
                        }
                }
        }
        pepdna_con_put(con);
}
#endif

/*
 * TCP2TCP scenario
 * Forwarding from Right to Left INTERNET domain
 * ------------------------------------------------------------------------- */
void pepdna_con_ri2li_work(struct work_struct *work)
{
        struct pepdna_con *con = container_of(work, struct pepdna_con, r2l_work);
        int rc = 0;

        while (rconnected(con)) {
                if ((rc = pepdna_con_i2i_fwd(con->rsock, con->lsock)) <= 0) {
                        if (rc == -EAGAIN) //FIXME Handle -EAGAIN flood
                                break;
                        pepdna_con_close(con);
                        break;
                }
        }
        pepdna_con_put(con);
}

/*
 * TCP2TCP scenario
 * Forwarding from Left to Right INTERNET domain
 * ------------------------------------------------------------------------- */
void pepdna_con_li2ri_work(struct work_struct *work)
{
        struct pepdna_con *con = container_of(work, struct pepdna_con, l2r_work);
        int rc = 0;

        while (lconnected(con)) {
                if ((rc = pepdna_con_i2i_fwd(con->lsock, con->rsock)) <= 0) {
                        if (rc == -EAGAIN) //FIXME Handle -EAGAIN flood
                                break;
                        pepdna_con_close(con);
                        break;
                }
        }
        pepdna_con_put(con);
}

/* pepdna_con_data_ready - interrupt callback indicating the socket has data
 * The queued work is launched into ?
 * ------------------------------------------------------------------------- */
void pepdna_l2r_conn_data_ready(struct sock *sk)
{
        struct pepdna_con *con = NULL;

        pep_debug("data ready on left side");
        read_lock_bh(&sk->sk_callback_lock);
        con = sk->sk_user_data;
        if (lconnected(con)) {
                pepdna_con_get(con);
                if (!queue_work(con->server->l2r_wq, &con->l2r_work)) {
                        pep_debug("l2r_work was already on a queue");
                        pepdna_con_put(con);
                }
        }
        read_unlock_bh(&sk->sk_callback_lock);
}

/* pepdna_con_data_ready - interrupt callback indicating the socket has data
 * The queued work is launched into ?
 * ------------------------------------------------------------------------- */
void pepdna_r2l_conn_data_ready(struct sock *sk)
{
        struct pepdna_con *con = NULL;

        read_lock_bh(&sk->sk_callback_lock);
        con = sk->sk_user_data;
        if (rconnected(con)) {
                pepdna_con_get(con);
                if (!queue_work(con->server->r2l_wq, &con->r2l_work)) {
                        pep_debug("r2l_work was already on a queue");
                        pepdna_con_put(con);
                }
        }
        read_unlock_bh(&sk->sk_callback_lock);
}

static unsigned int pepdna_pre_hook(void *priv, struct sk_buff *skb,
                const struct nf_hook_state *state)
{
        struct pepdna_con *con = NULL;
        struct syn_tuple *syn  = NULL;
        const struct iphdr *iph;
        const struct tcphdr *tcph;
        uint32_t hash_id = 0;

        if (!skb)
                return NF_ACCEPT;
        iph = ip_hdr(skb);
        if (iph->protocol == IPPROTO_TCP) {
                tcph = tcp_hdr(skb);
                /* Check for packets with ONLY SYN flag set */
                if (tcph->syn == 1 && tcph->ack == 0 && tcph->rst == 0) {

                        hash_id = pepdna_hash32_rjenkins1_2(iph->saddr, tcph->source);

                        con = pepdna_con_find(hash_id);
                        if (!con) {
                                syn = (struct syn_tuple *)kzalloc(sizeof(struct syn_tuple),
                                                GFP_ATOMIC);
                                if (!syn) {
                                        pep_err("kzalloc SYN -ENOMEM");
                                        return NF_DROP;
                                }
                                syn->saddr  = iph->saddr;
                                syn->source = tcph->source;
                                syn->daddr  = iph->daddr;
                                syn->dest   = tcph->dest;

                                con = pepdna_con_alloc(syn, skb, 0);
                                if (!con) {
                                        pep_err("pepdna_con_alloc failed");
                                        kfree(syn);
                                        return NF_DROP;
                                }
                                kfree(syn);
                                consume_skb(skb);
                                return NF_STOLEN;
                        }
                }
        }

        return NF_ACCEPT;
}

static const struct nf_hook_ops pepdna_nf_ops[] = {
        {
                .hook     = pepdna_pre_hook,
                .pf       = NFPROTO_IPV4,
                .hooknum  = NF_INET_PRE_ROUTING,
                .priority = NF_PEPDNA_PRI,
        },
};

/*
 * Start TCP-TCP task
 * This function is called by pepdna_server_start() @'server.c'
 * --------------------------------------------------------------------------*/
static int pepdna_i2i_start(struct pepdna_server *srv)
{
        int rc = 0;

        INIT_WORK(&srv->accept_work, pepdna_acceptor_work);

        rc = pepdna_work_init(srv);
        if (rc < 0)
                return rc;

        rc = pepdna_tcp_listen_init(srv);
        if (rc < 0) {
                pepdna_work_stop(srv);
                return rc;
        }

        nf_register_net_hooks(&init_net, pepdna_nf_ops,
                        ARRAY_SIZE(pepdna_nf_ops));
        return 0;
}

#ifdef CONFIG_PEPDNA_RINA
/*
 * Start RINA-TCP task
 * This function is called by pepdna_server_start() @'server.c'
 * --------------------------------------------------------------------------*/
static int pepdna_r2i_start(struct pepdna_server *srv)
{
        int rc = pepdna_netlink_init();
        if (rc < 0) {
                pep_err("Couldn't init Netlink socket");
                return rc;
        }

        rc = pepdna_work_init(srv);
        if (rc < 0) {
                pepdna_netlink_stop();
                return rc;
        }

        return 0;
}

/*
 * Start TCP-RINA task
 * This function is called by pepdna_server_start() @'server.c'
 * ------------------------------------------------------------------------- */
static int pepdna_i2r_start(struct pepdna_server *srv)
{
        int rc = 0;

        INIT_WORK(&srv->accept_work, pepdna_acceptor_work);

        rc = pepdna_netlink_init();
        if (rc < 0) {
                pep_err("Couldn't init Netlink socket");
                return rc;
        }

        rc = pepdna_work_init(srv);
        if (rc < 0) {
                pepdna_netlink_stop();
                return rc;
        }

        rc = pepdna_tcp_listen_init(srv);
        if (rc < 0) {
                pepdna_netlink_stop();
                pepdna_work_stop(srv);
                return rc;
        }

        nf_register_net_hooks(&init_net, pepdna_nf_ops,
                        ARRAY_SIZE(pepdna_nf_ops));
        return 0;
}

/*
 * Start RINA-RINA task
 * This function is called by pepdna_server_start() @'server.c'
 * --------------------------------------------------------------------------*/
static int pepdna_r2r_start(struct pepdna_server *srv)
{
        /* int rc = 0; */

        /* INIT_WORK(&srv->accept_work, pepdna_acceptor_work); */

        /* rc = pepdna_work_init(srv); */
        /* if (rc < 0) */
        /*         return rc; */

        /* rc = pepdna_create_listener(srv); */
        /* if (rc < 0) { */
        /*         pepdna_work_stop(srv); */
        /*         return rc; */
        /* } */

        return 0;
}
#endif

#ifdef CONFIG_PEPDNA_CCN
/*
 * Start TCP-CCN task
 * This function is called by pepdna_server_start() @'server.c'
 * --------------------------------------------------------------------------*/
static int pepdna_i2c_start(struct pepdna_server *srv)
{
        int rc = 0;

        INIT_WORK(&srv->accept_work, pepdna_acceptor_work);

        rc = pepdna_work_init(srv);
        if (rc < 0)
                return rc;

        rc = pepdna_tcp_listen_init(srv);
        if (rc < 0) {
                pepdna_work_stop(srv);
                return rc;
        }

        nf_register_net_hooks(&init_net, pepdna_nf_ops,
                        ARRAY_SIZE(pepdna_nf_ops));
        return 0;
}

/*
 * Start CCN-TCP task
 * This function is called by pepdna_server_start() @'server.c'
 * --------------------------------------------------------------------------*/
static int pepdna_c2i_start(struct pepdna_server *srv)
{
	/* TODO: Not yet implemented! */
        /* int rc = 0; */

        /* INIT_WORK(&srv->accept_work, pepdna_acceptor_work); */

        /* rc = pepdna_work_init(srv); */
        /* if (rc < 0) */
        /*         return rc; */

        /* rc = pepdna_tcp_listen_init(srv); */
        /* if (rc < 0) { */
        /*         pepdna_work_stop(srv); */
        /*         return rc; */
        /* } */

        /* nf_register_net_hooks(&init_net, pepdna_nf_ops, */
        /*                 ARRAY_SIZE(pepdna_nf_ops)); */
        return 0;
}

/*
 * Start CCN-CCN task
 * This function is called by pepdna_server_start() @'server.c'
 * --------------------------------------------------------------------------*/
static int pepdna_c2c_start(struct pepdna_server *srv)
{
	/* TODO: Not yet implemented! */
        /* int rc = 0; */

        /* INIT_WORK(&srv->accept_work, pepdna_acceptor_work); */

        /* rc = pepdna_work_init(srv); */
        /* if (rc < 0) */
        /*         return rc; */

        /* rc = pepdna_tcp_listen_init(srv); */
        /* if (rc < 0) { */
        /*         pepdna_work_stop(srv); */
        /*         return rc; */
        /* } */

        /* nf_register_net_hooks(&init_net, pepdna_nf_ops, */
        /*                 ARRAY_SIZE(pepdna_nf_ops)); */
        return 0;
}
#endif

static int init_pepdna_server(struct pepdna_server *srv)
{
        pepdna_srv = srv;
        srv->mode = pepdna_mode;

        srv->listener  = NULL;
        srv->accept_wq = NULL;
        srv->tcfa_wq   = NULL;
        srv->l2r_wq    = NULL;
        srv->r2l_wq    = NULL;

        srv->idr_in_use = 0;
        srv->port = port;
        hash_init(srv->htable);

        return 0;
}

/*
 * PEP-DNA server start
 * This function is called by module_init() @'core.c'
 * ------------------------------------------------------------------------- */
int pepdna_server_start(void)
{
        int rc = 0;
        struct pepdna_server *srv = kzalloc(sizeof(struct pepdna_server),
					    GFP_ATOMIC);
        if (!srv) {
                pep_err("Couldn't allocate memory for pepdna_server");
                return -ENOMEM;
        }

        rc = init_pepdna_server(srv);
        if (rc)
                return rc;

        switch (srv->mode) {
                case TCP2TCP:
                        rc = pepdna_i2i_start(srv);
                        if (rc < 0)
                                return rc;
                        break;
#ifdef CONFIG_PEPDNA_RINA
                case TCP2RINA:
                        rc = pepdna_i2r_start(srv);
                        if (rc < 0)
                                return rc;
                        break;
                case RINA2TCP:
                        rc = pepdna_r2i_start(srv);
                        if (rc < 0)
                                return rc;
                        break;
                case RINA2RINA:
                        rc = pepdna_r2r_start(srv);
                        if (rc < 0)
                                return rc;
                        break;
#endif
#ifdef CONFIG_PEPDNA_CCN
                case TCP2CCN:
                        rc = pepdna_i2c_start(srv);
                        if (rc < 0)
                                return rc;
                        break;
                case CCN2TCP:
                        rc = pepdna_c2i_start(srv);
                        if (rc < 0)
                                return rc;
                        break;
		case CCN2CCN:
                        rc = pepdna_c2c_start(srv);
                        if (rc < 0)
                                return rc;
                        break;
#endif
                default:
                        pep_err("pepdna_mode undefined");
                        return -EINVAL;
        }

        return rc;
}

/*
 * PEPDNA server stop
 * This function is called by module_exit() @'core.c'
 * ------------------------------------------------------------------------- */
void pepdna_server_stop(void)
{
        struct socket *sock = pepdna_srv->listener;
        struct pepdna_con *con = NULL;
        struct hlist_node *n;

        /* 1. First, we unregister NF_HOOK to stop processing new SYNs */
        if (pepdna_srv->mode < 2)
                nf_unregister_net_hooks(&init_net, pepdna_nf_ops,
                                ARRAY_SIZE(pepdna_nf_ops));

        /* 2. Check for connections which are still alive and destroy them */
        if (pepdna_srv->idr_in_use > 0) {
                hlist_for_each_entry_safe(con, n, pepdna_srv->htable, hlist) {
                        if (con) {
                                pep_err("Hmmm, %d con. still alive",
                                                pepdna_srv->idr_in_use);
                                pepdna_con_close(con);
                        }
                }
        }

        /* 3. Release main listening socket and Netlink socket */
        pepdna_netlink_stop();
        pepdna_srv->listener = NULL;
        pepdna_tcp_listen_stop(sock, &pepdna_srv->accept_work);

        /* 4. Flush and Destroy all works */
        pepdna_work_stop(pepdna_srv);

        /* 5. kfree PEPDNA server struct */
        kfree(pepdna_srv);
        pepdna_srv = NULL;

        pep_info("PEP-DNA kernel module unloaded");
}
