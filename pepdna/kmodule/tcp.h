/*
 *  pep-dna/pepdna/kmodule/tcp.h: Header file for PEP-DNA TCP support
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

#ifndef _PEPDNA_TCP_H
#define _PEPDNA_TCP_H

#include "connection.h"
#include "tcp_utils.h"

#define PEPDNA_SOCK_MARK 333

/* tcp_listen.c */
int pepdna_tcp_listen_init(struct pepdna_server *);
void pepdna_tcp_listen_stop(struct socket *, struct work_struct *);
void pepdna_acceptor_work(struct work_struct *work);

/* tcp_connect.c */
void pepdna_tcp_connect(struct work_struct *);

/* tcp.c */
int pepdna_con_i2i_fwd(struct socket *, struct socket *);

#endif /* _PEPDNA_TCP_H */
