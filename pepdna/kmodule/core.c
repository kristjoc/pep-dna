/*
 *  rina/pepdna/core.c: PEP-DNA core module
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
#include "tcp_utils.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>

/* START of Module Parameters */

int port = 0;
module_param(port, int, 0644);
MODULE_PARM_DESC(port, "TCP port for listen()");

int pepdna_mode = -1;
module_param(pepdna_mode, int, 0644);
MODULE_PARM_DESC(pepdna_mode,
     "TCP2TCP | TCP2RINA | TCP2CCN | RINA2TCP | RINA2RINA | CCN2TCP | CCN2CCN");

/* END of Module Parameters */

int sysctl_pepdna_sock_rmem[3] __read_mostly;              /* min/default/max */
int sysctl_pepdna_sock_wmem[3] __read_mostly;              /* min/default/max */

static const char* get_mode_name(void)
{
	switch (pepdna_mode) {
		case 0:  return "TCP2TCP";
		case 1:  return "TCP2RINA";
		case 2:  return "TCP2CCN";
		case 3:  return "RINA2TCP";
		case 4:  return "RINA2RINA";
		case 5:  return "CCN2TCP";
		case 6:  return "CCN2CCN";
		default: return "ERROR_MODE";
	}
}

static int __init pepdna_init(void)
{
	int rc = 0;

	sysctl_pepdna_sock_rmem[0] = RCVBUF_MIN;
	sysctl_pepdna_sock_rmem[1] = RCVBUF_DEF;
	sysctl_pepdna_sock_rmem[2] = RCVBUF_MAX;

	sysctl_pepdna_sock_wmem[0] = SNDBUF_MIN;
	sysctl_pepdna_sock_wmem[1] = SNDBUF_DEF;
	sysctl_pepdna_sock_wmem[2] = SNDBUF_MAX;

	rc = pepdna_register_sysctl();
	if (rc) {
		pr_err("Register sysctl failed with err. %d", rc);
		return rc;
	}

	rc = pepdna_server_start();
	if (rc < 0)
		pr_err("PEP-DNA LKM loaded with errors");

	pep_info("PEP-DNA LKM loaded succesfully in %s mode", get_mode_name());
	return rc;
}

static void __exit pepdna_exit(void)
{
	pepdna_server_stop();
	pepdna_unregister_sysctl();
}

/*
 * register init/exit functions
 */
module_init(pepdna_init);
module_exit(pepdna_exit);

/* module metadata */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kr1stj0n C1k0");
MODULE_VERSION(PEPDNA_MOD_VER);
MODULE_DESCRIPTION("PEP-DNA kernel module");
