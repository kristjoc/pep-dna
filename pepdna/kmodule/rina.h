/*
 *  rina/pepdna/rina.h: Header file for PEP-DNA RINA support
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

#ifndef _PEPDNA_RINA_H
#define _PEPDNA_RINA_H

#include "kfa.h"         /* included for struct ipcp_flow */
#include "kipcm.h"       /* default_kipcm */
#include "rds/rfifo.h"   /* rfifo_is_empty */

struct pepdna_con;

/* timeout for RINA flow poller in usesc */
#define FLOW_POLL_TIMEOUT 100

#define IRQ_BARRIER                                                     \
        do {                                                            \
                if (in_interrupt()) {                                   \
                        BUG();                                          \
                }                                                       \
        } while (0)

/* Exported Symbols from IRATI kernel modules */
extern int kfa_flow_du_read(struct kfa  *, int32_t, struct du **, size_t, bool);
extern struct ipcp_instance *kipcm_find_ipcp(struct kipcm *, uint16_t);
extern struct ipcp_flow *kfa_flow_find_by_pid(struct kfa *, int32_t);
extern unsigned char *du_buffer(const struct du *);
extern struct kfa *kipcm_kfa(struct kipcm *);
extern bool rfifo_is_empty(struct rfifo *);
extern bool is_du_ok(const struct du *);
extern struct du *du_create(size_t);
extern struct kipcm *default_kipcm;

int  pepdna_flow_write(struct ipcp_flow *, int, unsigned char *, size_t);
bool flow_is_ready(struct pepdna_con *);
bool queue_is_ready(struct ipcp_flow *);
long pepdna_wait_for_sdu(struct ipcp_flow *);
bool flow_is_ok(struct ipcp_flow *);
void pepdna_flow_alloc(struct work_struct *);
int  pepdna_con_i2r_fwd(struct pepdna_con *);
int  pepdna_con_r2i_fwd(struct pepdna_con *);

#endif /* _PEPDNA_RINA_H */
