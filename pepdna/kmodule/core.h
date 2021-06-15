/*
 *  pep-dna/pepdna/kmodule/core.h: PEP-DNA header with global declarations
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

#ifndef _PEPDNA_CORE_H
#define _PEPDNA_CORE_H

#include <linux/netdevice.h>

#define PEPDNA_MOD_VER "0.3.0"

extern int sysctl_pepdna_sock_rmem[3] __read_mostly;
extern int sysctl_pepdna_sock_wmem[3] __read_mostly;

#ifdef CONFIG_PEPDNA_DEBUG
#define pep_debug(fmt, args...) pr_debug("pepdna[DBG] %s(): " fmt"\n", \
                                         __func__ , ##args)
#else
/* do nothing instead of pr_debug() */
static inline __printf(1, 2)
void pep_debug(char *fmt, ...)
{
}
#endif

#define pep_err(fmt, args...) pr_err("pepdna[ERR] %s(): " fmt"\n", \
                                     __func__ , ##args)
#define pep_info(fmt, args...) pr_info("pepdna[INFO] %s(): " fmt"\n", \
                                       __func__ , ##args)

#ifdef CONFIG_SYSCTL
int pepdna_register_sysctl(void);
void pepdna_unregister_sysctl(void);
#else
#define pepdna_register_sysctl() 0
#define pepdna_unregister_sysctl()
#endif

#endif /* _PEPDNA_CORE_H */
