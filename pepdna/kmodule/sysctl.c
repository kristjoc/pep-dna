/*
 *  pep-dna/pepdna/kmodule/sysctl.c: sysctl interface to PEPDNA subsystem
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

#include <linux/sysctl.h>

static struct ctl_table_header *pepdna_ctl_hdr;

static struct ctl_table table[] = {
	{
		.procname     = "pepdna_sock_rmem",
		.data	      = &sysctl_pepdna_sock_rmem,
		.maxlen	      = sizeof(sysctl_pepdna_sock_rmem),
		.mode	      = 0644,
		.proc_handler = proc_dointvec,
	},
	{
		.procname     = "pepdna_sock_wmem",
		.data	      = &sysctl_pepdna_sock_wmem,
		.maxlen	      = sizeof(sysctl_pepdna_sock_wmem),
		.mode	      = 0644,
		.proc_handler = proc_dointvec,
	},
	{}
};

/*
 * Register net_sysctl
 * This function is called by pepdna_init() @'core.c'
 * ---------------------------------------------------------------------------*/
int pepdna_register_sysctl(void)
{
	pepdna_ctl_hdr = register_net_sysctl(&init_net, "net/pepdna", table);
	if (pepdna_ctl_hdr == NULL)
		return -ENOMEM;

	return 0;
}

/*
 * Unregister net_sysctl
 * This function is called by pepdna_exit() @'core.c'
 * ---------------------------------------------------------------------------*/
void pepdna_unregister_sysctl(void)
{
	unregister_net_sysctl_table(pepdna_ctl_hdr);
}
