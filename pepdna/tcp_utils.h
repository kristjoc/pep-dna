/*
 *  rina/pepdna/utils.h: Header files for PEP-DNA TCP related utilities
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

#ifndef _PEPDNA_TCP_UTILS_H
#define _PEPDNA_TCP_UTILS_H

#include <linux/types.h> /* included for unsigned int     */
#include <linux/net.h>   /* included for struct socket    */
#include <linux/in.h>    /* included for struct in_addr   */
#include <net/sock.h>    /* included for struct sock      */


/* Socket receive buffer sizes */
#define RCVBUF_MIN 8388608
#define RCVBUF_DEF 8388608
#define RCVBUF_MAX 8388608

/* Socket send buffer sizes */
#define SNDBUF_MIN 8388608
#define SNDBUF_DEF 8388608
#define SNDBUF_MAX 8388608

/* timeout for wait_to_send after -EAGAIN */
#define CONN_POLL_TIMEOUT 100

int pepdna_sock_write(struct socket *, unsigned char *, size_t);
void pepdna_tcp_nodelayedack(struct socket *);
void pepdna_ip_transparent(struct socket *);
void pepdna_set_mark(struct socket *, int);
void pepdna_tcp_nonagle(struct socket *);
void pepdna_set_bufsize(struct socket *);
uint32_t identify_client(struct socket *);
const char *inet_ntoa(struct in_addr *);
uint32_t inet_addr(char *ip);

#endif /* _PEPDNA_TCP_UTILS_H */
