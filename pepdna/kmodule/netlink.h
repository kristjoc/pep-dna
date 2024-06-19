/*
 *  pep-dna/kmodule/netlink.h: Header file for PEP-DNA Netlink code
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

#ifndef _PEPDNA_NETLINK_H
#define _PEPDNA_NETLINK_H

#include <linux/skbuff.h>

#define NL_PEPDNA_PROTO 31
#define NETLINK_MSS 21
#define NL_SEND_RETRIES 3
#define NL_RCVBUF_DEF 8388608
#define NL_SNDBUF_DEF 8388608
#define NL_SOCK_SNDBUF_LOCK 1
#define NL_SOCK_RCVBUF_LOCK 2

/**
 * struct nl_msg - netlink struct we send to fallocator
 * @saddr: source IP address
 * @source: source TCP port
 * @daddr: destination IP address
 * @dest: destination TCP port
 * @id: connection id
 * @port_id: port id of RINA flow
 * @alloc: 1: allocate / 0: deallocate
 */
struct nl_msg {
	u32 saddr;
	u16 source;
	u32 daddr;
	u16 dest;
	u32 id;
	int port_id;
	bool alloc;
} __attribute__ ((packed));

int  pepdna_netlink_init(void);
int  pepdna_nl_sendmsg(__be32, __be16, __be32, __be16,  uint32_t, int,  bool);
void pepdna_netlink_stop(void);

#endif /* _PEPDNA_NETLINK_H */
