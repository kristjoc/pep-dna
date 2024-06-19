/*
 *  pep-dna/kmodule/tcp_connet.c: PEP-DNA TCP connect()
 *
 *  Copyright (C) 2023  Kristjon Ciko <kristjoc@ifi.uio.no>
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
#include "netlink.h"
#include "tcp_utils.h"

#ifdef CONFIG_PEPDNA_RINA
#include "rina.h"
#endif

#ifdef CONFIG_PEPDNA_MINIP
#include "minip.h"
#endif

#ifdef CONFIG_PEPDNA_LOCAL_SENDER
#include <net/ip.h>
#endif

/*
 * TCP2TCP | RINA2TCP | MINIP2TCP  scenarios
 * Connect to TCP|RINA server upon accepting the connection
 * ------------------------------------------------------------------------- */
void pepdna_tcp_connect(struct work_struct *work)
{
	struct pepdna_con *con = container_of(work, struct pepdna_con, tcfa_work);
	struct sockaddr_in saddr = {0}, daddr = {0};
	struct socket *sock = NULL;
	struct sock *sk = NULL;
	const char *from, *to;
	int rc = 0;

	/* 1. Create socket */
	if (sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock) < 0) {
		pep_err("sock_create_kern returned %d", rc);
		goto err;
	}

	/* Source and Destination addresses */
	saddr.sin_family      = AF_INET;
	saddr.sin_addr.s_addr = con->tuple.saddr;
	saddr.sin_port        = con->tuple.source;

	daddr.sin_family      = AF_INET;
#ifdef CONFIG_PEPDNA_LOCAL_RECEIVER
	daddr.sin_addr.s_addr = (__force u32)htonl(INADDR_LOOPBACK);
#else
	daddr.sin_addr.s_addr = con->tuple.daddr;
#endif
	daddr.sin_port        = con->tuple.dest;

	/* 3. Tune TCP and set socket options */
	sock->sk->sk_reuse = SK_CAN_REUSE;     /* #defined SK_CAN_REUSE  1*/
	pepdna_set_bufsize(sock);
	pepdna_tcp_nonagle(sock);
	pepdna_tcp_nodelayedack(sock);
	/* Set IP_TRANSPARENT sock option so that we can bind original nonlocal IP
	 * address and TCP port in order to spoof client */
#ifndef CONFIG_PEPDNA_LOCAL_SENDER
	pepdna_ip_transparent(sock);
#endif
	/* Mark the socket with 333 MARK. This is only used when PEPDNA is at the
	 * same host as the sender/receiver or CCN relay */
#if defined(CONFIG_PEPDNA_LOCAL_SENDER) || defined(CONFIG_PEPDNA_LOCAL_RECEIVER)
	pepdna_set_mark(sock, PEPDNA_SOCK_MARK);
#endif

#ifndef CONFIG_PEPDNA_LOCAL_SENDER
	/* 4. Bind before connect to spoof source IP and TCP port */
	rc = kernel_bind(sock, (struct sockaddr*)&saddr, sizeof(saddr));
	if (rc < 0) {
		pep_err("kernel_bind %d", rc);
		sock_release(sock);
		goto err;
	}
#endif
	/* 5. Connect to Target Host */
	/* Use O_NONBLOCK flag if you don't want to wait for the right
           TCP connection establishment */
	rc = kernel_connect(sock, (struct sockaddr*)&daddr, sizeof(daddr), 0);
	if (rc < 0 && (rc != -EINPROGRESS)) {
		pep_err("kernel_connect failed %d", rc);
		if (sock) {
			kernel_sock_shutdown(sock, SHUT_RDWR);
			sock_release(sock);
		}
		goto err;
	}
	from = inet_ntoa(&(saddr.sin_addr));
	to   = inet_ntoa(&(daddr.sin_addr));
	pep_dbg("pepdna established %s:%d -> %s:%d connection",
		  from, ntohs(saddr.sin_port), to, ntohs(daddr.sin_port));
	kfree(from); kfree(to);

	if (con->server->mode == TCP2TCP) {
		/* Register callbacks for right socket */
		con->rsock = sock;
		sk = sock->sk;
		write_lock_bh(&sk->sk_callback_lock);
		sk->sk_data_ready = pepdna_r2l_conn_data_ready;
		sk->sk_user_data  = con;
		write_unlock_bh(&sk->sk_callback_lock);

		/* At this point, right TCP connection is established.
		 * Reinject SYN in back in the stack so that the left
		 * TCP connection can be established There is no need
		 * to set callbacks here for the left socket as
		 * pepdna_tcp_accept() will take care of it.
		 */
		pep_dbg("Reinjecting initial SYN packet");
#ifndef CONFIG_PEPDNA_LOCAL_SENDER
		netif_receive_skb(con->skb);
#else
		struct net *net = sock_net(con->server->listener->sk);
		ip_local_out(net, con->server->listener->sk, con->skb);
#endif
		return;
	}
#ifdef CONFIG_PEPDNA_RINA
	if (con->server->mode == RINA2TCP) {
		rc = pepdna_nl_sendmsg(con->tuple.saddr, con->tuple.source,
				       con->tuple.daddr, con->tuple.dest,
				       con->id, atomic_read(&con->port_id), 1);
		if (rc < 0) {
			pep_err("Couldn't ask to resume flow allocation");
			goto err;
		}
		/* Register callbacks for 'left' socket */
		con->lsock = sock;
		sk         = sock->sk;
		write_lock_bh(&sk->sk_callback_lock);
		sk->sk_data_ready = pepdna_l2r_conn_data_ready;
		sk->sk_user_data  = con;
		write_unlock_bh(&sk->sk_callback_lock);
	}
#endif
#ifdef CONFIG_PEPDNA_MINIP
	if (con->server->mode == MINIP2TCP) {
		rc = pepdna_minip_send_response(con->id, con->server->to_mac);
		if (rc < 0) {
			pep_err("Failed to send MINIP_CONN_RESPONSE");
			goto err;
                }

                con->state = ESTABLISHED;
                con->next_seq++;
                atomic_inc(&con->last_acked);
                con->next_recv++;

		/* Register callbacks for 'left' socket */
		con->lsock = sock;
		sk         = sock->sk;
		write_lock_bh(&sk->sk_callback_lock);
		sk->sk_data_ready = pepdna_l2r_conn_data_ready;
		sk->sk_user_data  = con;
		write_unlock_bh(&sk->sk_callback_lock);

		/* At this point, the right TCP connection is established */
		atomic_set(&con->lflag, 1);
		/* Wake up 'left' socket */
		con->lsock->sk->sk_data_ready(con->lsock->sk);
	}
#endif
	return;
err:
	pepdna_con_close(con);
}
