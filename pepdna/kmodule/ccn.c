/*
 *  pep-dna/pepdna/kmodule/ccn.c: PEP-DNA CCN support
 *
 *  Copyright (C) 2021  Kristjon Ciko <kristjoc@ifi.uio.no>
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

#ifdef CONFIG_PEPDNA_CCN
#include "ccn.h"
#include "core.h"
#include "tcp.h"

#include "ccnl-prefix.h"
#include "ccnl-face.h"
#include "ccnl-pkt.h"
#include "ccnl-pkt-builder.h"

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)
#include <linux/signal.h>
#else
#include <linux/sched/signal.h>
#endif

#include "../../ccn-lite/src/ccnl-core/src/ccnl-prefix.c"
#include "../../ccn-lite/src/ccnl-pkt/src/ccnl-pkt-builder.c"

/* These shouldn't be hardcoded */
int suite = CCNL_SUITE_NDNTLV;
unsigned int chunknum = UINT_MAX;
bool request_done = false;

/*
 *  Parse Request from TCP client
 * -------------------------------------------------------------------------- */
static void parse_request(unsigned char *request, char *content)
{
	char *pos = strstr((const char *)request, "GET");
	if (pos) {
		sscanf(pos, "%*s %s", content);
	}
}

/*
 *  Prepare and Forward the interest request to CCN relay
 * -------------------------------------------------------------------------- */
static int pepdna_send_to_ccn_relay(struct socket *sock, char *content)
{
	struct ccnl_buf_s *buf = NULL;
        struct msghdr msg = {
                .msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL,
        };
        struct kvec vec;
	int cnt, rc;

	struct ccnl_prefix_s *prefix = ccnl_URItoPrefixK(content, suite,
		chunknum == UINT_MAX ? NULL : &chunknum);

	for (cnt = 0; cnt < 3; cnt++) {
		int32_t nonce = (int32_t) prandom_u32();
		struct ccnl_face_s dummyFace;
		ccnl_interest_opts_u int_opts;
#ifdef USE_SUITE_NDNTLV
		int_opts.ndntlv.nonce = nonce;
#endif

		pep_debug("sending request, iteration %d", cnt);

		memset(&dummyFace, 0, sizeof(dummyFace));

		buf = ccnl_mkSimpleInterestK(prefix, &int_opts);
		if (!buf) {
			pep_err("Failed to create interest");
			return -1;	    // FIXME
		}

		pep_debug("interest has %zu bytes", buf->datalen);
		vec.iov_len = buf->datalen;
		vec.iov_base = (unsigned char *)buf->data;

		rc = kernel_sendmsg(sock, &msg, &vec, 1, buf->datalen);
                if (rc <= 0)
                        pep_err("Couldn't send to CCN relay");
	}
	return rc;
}

static int pepdna_fwd_request(struct socket *from, struct socket *to)
{
        struct msghdr msg = {
                .msg_flags = MSG_DONTWAIT,
        };
        struct kvec vec;
	char content[32] = {0};
        int rc = 0;
        /* allocate buffer memory */
        unsigned char *buffer = kzalloc(1024, GFP_KERNEL);
        if (!buffer) {
                pep_err("kzalloc buffer");
                return -ENOMEM;
        }
        vec.iov_base = buffer;
        vec.iov_len  = 1024;

        iov_iter_kvec(&msg.msg_iter, READ | ITER_KVEC, &vec, 1, vec.iov_len);
        rc = sock_recvmsg(from, &msg, MSG_DONTWAIT);
        if (rc > 0) {
		buffer[rc] = '\0';
		parse_request(buffer, content);

		rc = pepdna_send_to_ccn_relay(to, content);
		request_done = true;
	}

        kfree(buffer);
        return rc;
}

/*
 * TCP2CCN scenario
 * Prepare UDP socket to connect to CCN relay
 * ------------------------------------------------------------------------- */
void pepdna_udp_open(struct work_struct *work)
{
	struct pepdna_con *con = container_of(work, struct pepdna_con, tcfa_work);
	struct sockaddr_in saddr = {0};
	struct sockaddr_in daddr = {0};
	struct socket* sock      = NULL;
	struct sock* sk          = NULL;
	const char *str_ip       = NULL;
	int rc                   = 0;

	/* 1. Create socket */
	if (sock_create_kern(&init_net, AF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock) < 0) {
		pep_err("sock_create_kern failed %d", rc);
		return;
	}

	/* Source and Destination addresses */
	saddr.sin_family      = AF_INET;
	saddr.sin_addr.s_addr = con->tuple.saddr;
	saddr.sin_port        = con->tuple.source;

	daddr.sin_family      = AF_INET;
#ifdef CONFIG_PEPDNA_LOCALHOST
	daddr.sin_addr.s_addr = (__force u32)htonl(INADDR_LOOPBACK);
#else
	daddr.sin_addr.s_addr = con->tuple.daddr;
#endif
	daddr.sin_port        = con->tuple.dest;

	/* 3. Tune TCP and set socket options */
	sock->sk->sk_reuse = SK_CAN_REUSE;     /* #defined SK_CAN_REUSE  1*/
	pepdna_set_bufsize(sock);
	/* Set IP_TRANSPARENT sock option so that we can bind original nonlocal IP
	 * address and TCP port in order to spoof client */
	pepdna_ip_transparent(sock);
	/* Mark the socket with 333 MARK. This is only used when PEPDNA is at the
	 * same host as the server or CCN relay */
#ifdef CONFIG_PEPDNA_LOCALHOST
	pepdna_set_mark(sock, PEPDNA_SOCK_MARK);
#endif

	/* 4. Bind to spoof source IP and Port */
	rc = kernel_bind(sock, (struct sockaddr*)&saddr, sizeof(saddr));
	if (rc < 0) {
		pep_err("kernel_bind %d", rc);
		sock_release(sock);
		goto err;
	}

	/* 5. Connect to Target Host */
	rc = kernel_connect(sock, (struct sockaddr*)&daddr, sizeof(daddr), 0);
	if (rc < 0) {
		pep_err("kernel_connect failed %d", rc);
		sock_release(sock);
		goto err;
	}
	str_ip = inet_ntoa(&(daddr.sin_addr));
	pep_debug("PEP-DNA prepared UDP sock to connect to %s:%d", str_ip,
		  ntohs(daddr.sin_port));
	kfree(str_ip);

        /* Register callbacks for right UDP socket */
        con->rsock = sock;
        sk = sock->sk;
        write_lock_bh(&sk->sk_callback_lock);
        sk->sk_data_ready = pepdna_udp_data_ready;
        sk->sk_user_data  = con;
        write_unlock_bh(&sk->sk_callback_lock);

        /* At this point, reinject SYN back in the stack so that the left
	 * TCP connection can be established
         * There is no need to set callbacks here for the left socket as
         * pepdna_tcp_accept() will take care of it.
         */
        pep_debug("Reinjecting initial SYN packet");
        netif_receive_skb(con->skb);
        return;
err:
    pepdna_con_put(con);
}

/* pepdna_udp_data_ready - interrupt callback indicating the socket has data
 * The queued work is launched into ?
 * ------------------------------------------------------------------------- */
void pepdna_udp_data_ready(struct sock *sk)
{
        struct pepdna_con *con = NULL;

        pep_debug("data ready from CCN");
        read_lock_bh(&sk->sk_callback_lock);
        con = sk->sk_user_data;
        if (con) {
                pepdna_con_get(con);
                if (!queue_work(con->server->r2l_wq, &con->r2l_work)) {
                        pep_debug("r2l_work was already on a queue");
                        pepdna_con_put(con);
                }
        }
        read_unlock_bh(&sk->sk_callback_lock);
}

/*
 * CCN2TCP scenario
 * Forwarding from Right CCN domain to Left INTERNET domain
 * ------------------------------------------------------------------------- */
void pepdna_con_c2i_work(struct work_struct *work)
{
        struct pepdna_con *con = container_of(work, struct pepdna_con, l2r_work);
        int rc = 0;

        while (lconnected(con)) {
                if ((rc = pepdna_con_i2i_fwd(con->rsock, con->lsock)) <= 0) {
                        if (rc == -EAGAIN) /* FIXME: Handle -EAGAIN flood */
                                break;
                        pepdna_con_close(con);
                        break;
                }
        }
        pepdna_con_put(con);
}

/*
 * TCP2CCN scenario
 * Forwarding from Left INTERNET domain to Right CCN domain
 * ------------------------------------------------------------------------- */
void pepdna_con_i2c_work(struct work_struct *work)
{
        struct pepdna_con *con = container_of(work, struct pepdna_con, l2r_work);
        int rc = 0;

        while (lconnected(con)) {
		if (unlikely(!request_done)) {
			rc = pepdna_fwd_request(con->lsock, con->rsock);
			if (rc <= 0) {
				pepdna_con_close(con);
				break;
			}
		} else {
			if ((rc = pepdna_con_i2i_fwd(con->lsock, con->rsock)) <= 0) {
				if (rc == -EAGAIN) /* FIXME: Handle -EAGAIN flood */
					break;
				pepdna_con_close(con);
				break;
			}
		}
        }
        pepdna_con_put(con);
}
#endif
