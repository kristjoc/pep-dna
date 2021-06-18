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

#include "server.h"
#include "core.h"
#include "tcp.h"
#include "tcp_utils.h"

/*
 * Forward data from one TCP socket to another
 * ------------------------------------------------------------------------- */
int pepdna_con_i2i_fwd(struct socket *from, struct socket *to)
{
        struct msghdr msg = {
                .msg_flags = MSG_DONTWAIT,
        };
        struct kvec vec;
        int rc = 0, rs        = 0;
        /* allocate buffer memory */
        unsigned char *buffer = kzalloc(MAX_BUF_SIZE, GFP_KERNEL);
        if (!buffer) {
                pep_err("kzalloc buffer");
                return -ENOMEM;
        }
        vec.iov_base = buffer;
        vec.iov_len  = MAX_BUF_SIZE;

        iov_iter_kvec(&msg.msg_iter, READ | ITER_KVEC, &vec, 1, vec.iov_len);
        rc = sock_recvmsg(from, &msg, MSG_DONTWAIT);
        if (rc > 0) {
                rs = pepdna_sock_write(to, buffer, rc);
                if (rc <= 0) {
                        pep_err("Couldn't write to socket");
                }
        } else {
                if (rc == -EAGAIN || rc == -EWOULDBLOCK)
                        pep_debug("sock_recvmsg() says %d", rc);
        }

        kfree(buffer);
        return rc;
}
