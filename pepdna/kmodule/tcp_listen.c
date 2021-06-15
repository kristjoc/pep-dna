/*
 *  pep-dna/pepdna/kmodule/tcp_listen.c: PEP-DNA TCP listen()
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
#include "tcp.h"
#include "core.h"

#include <linux/module.h>

static void pepdna_tcp_listen_data_ready(struct sock *);
static int pepdna_tcp_accept(struct pepdna_server *);

/*
 * pepdna_listener_data_ready - interrupt callback with connection request
 * The queued job is launched into pepdna_acceptor_work()
 * ------------------------------------------------------------------------- */
static void pepdna_tcp_listen_data_ready(struct sock *sk)
{
        void (*ready)(struct sock *sk);

        pep_debug("pepdna listen data ready sk %p\n", sk);

        read_lock_bh(&sk->sk_callback_lock);
        ready = sk->sk_user_data;
        if (!ready) { /* check for teardown race */
                ready = sk->sk_data_ready;
	        goto out;
        }
        if (sk->sk_state == TCP_LISTEN) /* Q listen work only for listener */
                queue_work(pepdna_srv->accept_wq, &pepdna_srv->accept_work);
out:
        read_unlock_bh(&sk->sk_callback_lock);
        if (ready)
            ready(sk);
}

/*
 * PEPDNA create listener for incoming connection
 * This function is called by pepdna_i2x_start() @'server.c'
 * ------------------------------------------------------------------------- */
int pepdna_tcp_listen_init(struct pepdna_server *srv)
{
        struct socket *sock      = NULL;
        struct sock *sk          = NULL;
        struct sockaddr_in saddr = {0};
        int addr_len             = 0;
        int rc                   = 0;

        rc = sock_create_kern(&init_net, PF_INET, SOCK_STREAM, IPPROTO_TCP,
                              &sock);
        if (rc < 0) {
                pep_err("sock_create_kern %d", rc);
                return rc;
        }

        srv->listener = sock;
        sk = sock->sk;

        /* Tune TCP */
        sk->sk_reuse = SK_CAN_REUSE;             /* #defined SK_CAN_REUSE  1*/
        pepdna_set_bufsize(sock);
        pepdna_tcp_nonagle(sock);
        pepdna_tcp_nodelayedack(sock);
        pepdna_ip_transparent(sock);

        /* Register callback */
        write_lock_bh(&sk->sk_callback_lock);
        sk->sk_user_data  = sk->sk_data_ready;
        sk->sk_data_ready = pepdna_tcp_listen_data_ready;
        write_unlock_bh(&sk->sk_callback_lock);

        saddr.sin_family      = AF_INET;
        saddr.sin_addr.s_addr = INADDR_ANY;
        saddr.sin_port        = (__force u16)htons(srv->port);
        addr_len              = sizeof(saddr);

        rc = kernel_bind(sock, (struct sockaddr*)&saddr, addr_len);
        if (rc < 0) {
                pep_err("kernel_bind %d", rc);
                goto err;
        }
        pep_debug("listener bound to port %d", srv->port);

        rc = kernel_listen(sock, MAX_CONNS);
        if (rc < 0) {
                pep_err("kernel_listen %d ", rc);
                goto err;
        }
        pep_debug("PEPDNA is listening for incoming TCP connections");

        return 0;
err:
        if (sock)
                sock_release(sock);
        sock = NULL;
        return -EINVAL;
}

/*
 * Accept an incoming TCP connection
 * This function is called by pepdna_acceptor_work() @'tcp_listen.c'
 * ------------------------------------------------------------------------- */
static int pepdna_tcp_accept(struct pepdna_server *srv)
{
        struct socket *sock    = srv->listener;
        struct pepdna_con *con = NULL;
        struct socket *lsock   = NULL;
        struct sock *lsk       = NULL;
        struct sock *rsk       = NULL;
        uint32_t hash_id       = 0;
        int rc                 = 0;

        while (1) {
                rc = kernel_accept(sock, &lsock, O_NONBLOCK);
                if (rc < 0)
                        return rc;

                hash_id = identify_client(lsock);
                con     = pepdna_con_find(hash_id);
                if (!con) {
                        pep_err("con not found in Hash Table");
                        sock_release(lsock);
                        lsock = NULL;
                        rc = -1;
                        break;
                }
                pep_debug("PEPDNA accepted new connection with hash_id %u",
                                hash_id);

                /* Register callbacks for left sock and activate right sock */
                con->lsock = lsock;
                lsk        = lsock->sk;

                write_lock_bh(&lsk->sk_callback_lock);
                lsk->sk_data_ready = pepdna_l2r_conn_data_ready;
                lsk->sk_user_data  = con;
                atomic_set(&con->lflag, 1);
                atomic_set(&con->rflag, 1);
                write_unlock_bh(&lsk->sk_callback_lock);

                if (srv->mode == TCP2RINA) {
                        /* Queue RINA to INTERNET work right now */
                        if (!queue_work(srv->r2l_wq, &con->r2l_work)) {
                                pep_err("r2i_work was already on a queue");
                                pepdna_con_put(con);
                                return -1;
                        }
                }
                /* Wake up both sockets */
                lsk->sk_data_ready(lsk);
                if (con->rsock) {
                        rsk = con->rsock->sk;
                        rsk->sk_data_ready(rsk);
                }
        }

        return rc;
}

void pepdna_acceptor_work(struct work_struct *work)
{
        int rc = 0;
        struct pepdna_server *srv = container_of(work, struct pepdna_server,
                                                 accept_work);
        rc = pepdna_tcp_accept(srv);
        if (rc >=0)
                pep_debug("TCP accept() returned  %d", rc);
}

/*
 * Stop PEPDNA listening server
 * This function is called by pepdna_server_stop() @'server.c'
 * ------------------------------------------------------------------------- */
void pepdna_tcp_listen_stop(struct socket *sock, struct work_struct *acceptor)
{
        struct sock *sk;

        if (!sock)
                return;
        if (pepdna_srv->mode == RINA2TCP || pepdna_srv->mode == RINA2RINA)
                return;

        pep_debug("Stopping PEPDNA main sock listener");
        sk = sock->sk;

        /* serialize with and prevent further callbacks */
        lock_sock(sk);
        write_lock_bh(&sk->sk_callback_lock);
        if (sk->sk_user_data) {
                sk->sk_data_ready = sk->sk_user_data;
                sk->sk_user_data = NULL;
        }
        write_unlock_bh(&sk->sk_callback_lock);
        release_sock(sk);

        /* wait for accepts to stop and close the socket */
        flush_workqueue(pepdna_srv->accept_wq);
        flush_work(acceptor);
        sock_release(sock);
}
