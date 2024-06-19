/*
 *  pep-dna/kmodule/core.c: PEP-DNA core module
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

#include "core.h"
#include "server.h"
#include "tcp_utils.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>

/* START of Module Parameters */
int port = 0;
module_param(port, int, 0644);
MODULE_PARM_DESC(port, "PEP-DNA main TCP listening port");

int mode = -1;
module_param(mode, int, 0644);
MODULE_PARM_DESC(mode, "PEP-DNA operating mode (e.g., TCP2TCP, TCP2RINA, ...)");

#ifdef CONFIG_PEPDNA_MINIP
char *ifname = "eth0";
module_param(ifname, charp, 0644);
MODULE_PARM_DESC(ifname, "Interface name");

char *macstr = "ff:ff:ff:ff:ff:ff";
module_param(macstr, charp, 0644);
MODULE_PARM_DESC(macstr, "Ethernet MAC address of the peer PEP-DNA");
#endif
/* END of Module Parameters */

int sysctl_pepdna_sock_rmem[3] __read_mostly;	    /* min/default/max */
int sysctl_pepdna_sock_wmem[3] __read_mostly;	    /* min/default/max */

static const char* get_mode_name(void)
{
	switch (mode) {
	case 0:
		return "TCP2TCP";
	case 1:
		return "TCP2RINA";
	case 2:
		return "TCP2CCN";
	case 3:
		return "TCP2MINIP";
	case 4:
		return "RINA2TCP";
	case 5:
		return "MINIP2TCP";
	case 6:
		return "CCN2TCP";
	case 7:
		return "RINA2RINA";
	case 8:
		return "CCN2CCN";
	default:
		return "ERROR";
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
		pep_err("Unable to register sysctl");
		goto out;
	}

	rc = pepdna_server_start();
	if (rc < 0) {
		pepdna_unregister_sysctl();
		goto out;
	}

        pep_info("pepdna loaded in %s mode", get_mode_name());

	return 0;
out:
	pep_err("Unable to load pepdna in %s mode", get_mode_name());

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
MODULE_DESCRIPTION(PEPDNA_DESCRIPTION);
