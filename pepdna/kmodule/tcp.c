/*
 *  pep-dna/pepdna/kmodule/tcp.c: PEP-DNA TCP support
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

#include "core.h"
#include "server.h"
#include "tcp.h"
#include "tcp_utils.h"

/*
 * Forward data from one TCP socket to another
 * ------------------------------------------------------------------------- */
int pepdna_con_i2i_fwd(struct socket *from, struct socket *to)
{
	struct msghdr msg = {0};
	struct kvec vec;
	int read = 0, sent = 0;

	/* allocate buffer memory */
	unsigned char *buffer = kmalloc(MAX_BUF_SIZE, GFP_KERNEL);
	if (!buffer) {
		pep_err("kzalloc -ENOMEM");
		return -ENOMEM;
	}
	vec.iov_base = buffer;
	vec.iov_len  = MAX_BUF_SIZE;
	// Initialize msg structure
	msg.msg_flags = MSG_DONTWAIT;

	read = kernel_recvmsg(from, &msg, &vec, 1, vec.iov_len, MSG_DONTWAIT);
	if (likely(read > 0)) {
		sent = pepdna_sock_write(to, buffer, read);
		if (sent < 0) {
			pep_err("error forwarding to socket");
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
 * TCP2TCP scenario
 * Forwarding from Right to Left INTERNET domain
 * ------------------------------------------------------------------------- */
void pepdna_con_r2l_fwd(struct work_struct *work)
{
	struct pepdna_con *con = container_of(work, struct pepdna_con, r2l_work);
	int rc = 0;

	while (rconnected(con)) {
		if ((rc = pepdna_con_i2i_fwd(con->rsock, con->lsock)) <= 0) {
			if (rc == -EAGAIN) /* FIXME: Handle -EAGAIN flood */
				break;
			pepdna_con_close(con);
		}
	}
	pepdna_con_put(con);
}

/*
 * TCP2TCP scenario
 * Forwarding from Left to Right INTERNET domain
 * ------------------------------------------------------------------------- */
void pepdna_con_l2r_fwd(struct work_struct *work)
{
	struct pepdna_con *con = container_of(work, struct pepdna_con, l2r_work);
	int rc = 0;

	while (lconnected(con)) {
		if ((rc = pepdna_con_i2i_fwd(con->lsock, con->rsock)) <= 0) {
			if (rc == -EAGAIN) /* FIXME: Handle -EAGAIN flood */
				break;
			pepdna_con_close(con);
		}
	}
	pepdna_con_put(con);
}
