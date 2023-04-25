/*
 *  pep-dna/pepdna/kmodule/utils.c: PEP-DNA related utilities
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

#include "core.h"           /* core header                                */
#include "tcp_utils.h"      /* main header                                */
#include "hash.h"

#include <linux/kernel.h>   /* included for sprintf                       */
#include <linux/kthread.h>  /* included for kthread_should_stop           */
#include <linux/string.h>   /* included for memset                        */
#include <linux/delay.h>    /* included for usleep_range                  */
#include <linux/slab.h>     /* included for kmalloc                       */
#include <linux/wait.h>     /* included for wait_event_interruptible      */
#include <linux/net.h>      /* included for socket_wq                     */
#include <linux/version.h>  /* included for KERNEL_VERSION                */

#include <net/tcp.h>        /* included for TCP_NODELAY and TCP_QUICKACK  */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)
#include <linux/signal.h>
#else
#include <linux/sched/signal.h>
#endif

static void pepdna_wait_to_send(struct sock *);
static bool pepdna_sock_writeable(struct sock *);


/*
 * Disable Delayed-ACK algorithm
 * ------------------------------------------------------------------------- */
void pepdna_tcp_nodelayedack(struct socket *sock)
{
	pep_debug("Setting TCP_QUICKACK");

	tcp_sock_set_quickack(sock->sk);
}

/*
 * Disable Nagle Algorithm
 * doing it this way avoids calling tcp_sk()
 * ------------------------------------------------------------------------- */
void pepdna_tcp_nonagle(struct socket *sock)
{
	pep_debug("Disabling Nagle Algorithm");

	tcp_sock_set_nodelay(sock->sk);
}

/*
 * Because of certain restrictions in the IPv4 routing output code you'll have
 * to modify your application to allow it to send datagrams _from_ non-local IP
 * addresses. All you have to do is enable the (SOL_IP, IP_TRANSPARENT) socket
 * option before calling bind:
 * ------------------------------------------------------------------------- */
void pepdna_ip_transparent(struct socket *sock)
{
        int rc = 0, val = 1;

        rc = sock_common_setsockopt(sock, SOL_IP, IP_TRANSPARENT, (char *)&val,
				    sizeof(val));
        if (rc < 0)
                pep_err("Couldn't set IP_TRANSPARENT socket opt");
}

void pepdna_set_mark(struct socket *sock, int val)
{
        int rc = 0;

        rc = sock_common_setsockopt(sock, SOL_SOCKET, SO_MARK, (char *)&val,
				    sizeof(val));
        if (rc < 0)
                pep_err("Couldn't mark socket with mark %d", val);
}

/*
 * Set BUF size
 * quoting tcp(7):
 *   On individual connections, the socket buffer size must be set prior to the
 *   listen(2) or connect(2) calls in order to have it take effect.
 *   This is the wrapper to do so.
 * ------------------------------------------------------------------------- */
void pepdna_set_bufsize(struct socket *sock)
{
        struct sock *sk = sock->sk;
        unsigned int snd, rcv;

        snd = sysctl_pepdna_sock_wmem[1];
        rcv = sysctl_pepdna_sock_rmem[1];

        if (snd) {
                sk->sk_userlocks |= SOCK_SNDBUF_LOCK;
                sk->sk_sndbuf = snd;
        }
        if (rcv) {
                sk->sk_userlocks |= SOCK_RCVBUF_LOCK;
                sk->sk_rcvbuf = rcv;
        }
}

/*
 * Check if socket send buffer has space
 * ------------------------------------------------------------------------- */
static bool pepdna_sock_writeable(struct sock *sk)
{
        return sk_stream_is_writeable(sk);
}

/*
 * Wait for sock to become writeable
 * ------------------------------------------------------------------------- */
static void pepdna_wait_to_send(struct sock *sk)
{
        struct socket_wq *wq = NULL;
        long timeo = usecs_to_jiffies(CONN_POLL_TIMEOUT);

        rcu_read_lock();
        wq  = rcu_dereference(sk->sk_wq);
        rcu_read_unlock();

        do {
		wait_event_interruptible_timeout(wq->wait,
                                                 pepdna_sock_writeable(sk),
                                                 timeo);
        } while(!pepdna_sock_writeable(sk));
}

/*
 * Write buf of size_t len to TCP socket
 * Called by: pepdna_con_x2i_fwd()
 * ------------------------------------------------------------------------- */
int pepdna_sock_write(struct socket *sock, unsigned char *buf, size_t len)
{
        struct msghdr msg = {
                .msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL,
        };
        struct kvec vec;
	size_t left = len;
        size_t sent = 0;
        int count   = 0;
	int rc      = 0;

        while (left) {
                vec.iov_len = left;
                vec.iov_base = (unsigned char *)buf + sent;

                rc = kernel_sendmsg(sock, &msg, &vec, 1, left);
                if (rc > 0) {
                        sent += rc;
                        left -= rc;
                } else {
                        if (rc == -EAGAIN) {
                                pepdna_wait_to_send(sock->sk);
                                continue;
                        } else if (rc == 0) {
                                if (++count < 3) /* FIXME */
                                        continue;
                        }
                        return sent ? sent:rc;
                }
        }

        return sent;
}

/*
 * Convert IP address from struct in_addr type to string
 * ------------------------------------------------------------------------- */
const char *inet_ntoa(struct in_addr *in)
{
        uint32_t int_ip = 0;
        char *str_ip    = NULL;

        str_ip = kzalloc(16 * sizeof(char), GFP_KERNEL);
        if (!str_ip)
                return NULL;

        int_ip = in->s_addr;

        sprintf(str_ip, "%d.%d.%d.%d", (int_ip) & 0xFF,
                                       (int_ip >> 8) & 0xFF,
                                       (int_ip >> 16) & 0xFF,
                                       (int_ip >> 24) & 0xFF);
        /* It is the duty of the caller to free the memory allocated by this
         * function
         */
        return str_ip;
}

/*
 * Convert string IP address to in_addr type
 * ------------------------------------------------------------------------- */
uint32_t inet_addr(char *ip)
{
        int a, b, c, d;
        char arr[4];
        uint32_t *tmp;

        sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d);
        arr[0] = a; arr[1] = b; arr[2] = c; arr[3] = d;

        tmp = (uint32_t*)arr;
        return *tmp;
}

/*
 * Return the hash(saddr, source) of the connected socket
 * ------------------------------------------------------------------------- */
uint32_t identify_client(struct socket *sock)
{
        struct sockaddr_in *addr  = NULL;
        __be32 src_ip;
        __be16 src_port;
        uint32_t hash_id;
        int addr_len, rc = 0;

        addr = kzalloc(sizeof(struct sockaddr_in), GFP_KERNEL);
        if (!addr)
                return -ENOMEM;

        addr_len = sizeof(struct sockaddr_in);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,17,0)
        rc = sock->ops->getname(sock, (struct sockaddr *)addr, &addr_len, 2);
#else
        rc = sock->ops->getname(sock, (struct sockaddr *)addr, 2);
#endif
        if (rc < 0) {
                pep_err("getname %d", rc);
                kfree(addr);
                addr = NULL;
                goto err;
        }

        src_ip   = addr->sin_addr.s_addr;
        src_port = addr->sin_port;
        hash_id  = pepdna_hash32_rjenkins1_2(src_ip, src_port);

        kfree(addr);
        addr = NULL;

        return hash_id;
err:
        if (sock) {
                sock_release(sock);
                sock = NULL;
        }

        return rc;
}
