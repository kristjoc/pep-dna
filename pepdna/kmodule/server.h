/*
 *  pep-dna/pepdna/kmodule/server.h: PEP-DNA server infrastructure header
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

#ifndef _PEPDNA_SERVER_H
#define _PEPDNA_SERVER_H

#include <linux/workqueue.h>    /* work_struct, workqueue_struct, htable*/

#define MODULE_NAME      "PEP-DNA"
#define NF_PEPDNA_PRI    -500
#define PEPDNA_HASH_BITS 9
#define MAX_CONNS        65535
#define MAX_SDU_SIZE     1451
#define MAX_BUF_SIZE     1451 * 7

struct sock;
struct nl_msg;

enum server_mode {
	TCP2TCP = 0,
	TCP2RINA,
	TCP2CCN,
	RINA2TCP,
	RINA2RINA,
	CCN2TCP,
	CCN2CCN
};


/**
 * struct pepdna_server - PEP-DNA server struct
 * @mode:        TCP2TCP | TCP2RINA | TCP2CCN | RINA2TCP | RINA2RINA ...
 * @l2r_wq:      left2right translation workqueue
 * @r2l_wq:      right2left translation workqueue
 * @tcfa_wq:     TCP connect/RINA Flow Allocation workqueue
 * @accept_wq:   TCP accept workqueue
 * @accept_work: TCP accept work item
 * @listener:    pepdna listener socket
 * @port:        pepdna TCP listener port
 * @htable:      Hash table for connections
 * @idr_in_use:  amount of allocated identifier entry
 */
struct pepdna_server {
        enum server_mode mode;
        struct workqueue_struct *l2r_wq;
        struct workqueue_struct *r2l_wq;
        struct workqueue_struct *tcfa_wq;
        struct workqueue_struct *accept_wq;
        struct work_struct accept_work;
        struct socket *listener;
        int port;
        struct hlist_head htable[PEPDNA_HASH_BITS];
        int idr_in_use;
};

void pepdna_l2r_conn_data_ready(struct sock *);
void pepdna_r2l_conn_data_ready(struct sock *);
void pepdna_con_li2ri_work(struct work_struct *work);
void pepdna_con_ri2li_work(struct work_struct *work);
int  pepdna_server_start(void);
void pepdna_server_stop(void);
int  pepdna_work_init(struct pepdna_server *);
void pepdna_work_stop(struct pepdna_server *);

#endif /* _PEPDNA_SERVER_H */
