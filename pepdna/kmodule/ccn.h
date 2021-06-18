/*
 *  pep-dna/pepdna/kmodule/ccn.h: Header file for PEP-DNA CCN support
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

#ifndef _PEPDNA_CCN_H
#define _PEPDNA_CCN_H

#ifdef CONFIG_PEPDNA_CCN
#include <linux/workqueue.h>	/* included for struct work_struct */
#include <net/sock.h>		/* included for struct sock */

void pepdna_udp_open(struct work_struct *work);
void pepdna_forward_request(struct sock *sk);
void pepdna_udp_data_ready(struct sock *sk);
void pepdna_con_c2i_work(struct work_struct *work);
void pepdna_con_i2c_work(struct work_struct *work);
#endif

#endif /* _PEPDNA_CCN_H */
