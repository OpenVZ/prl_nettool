/*
 * Copyright (c) 2015-2017, Parallels International GmbH
 * Copyright (c) 2017-2022 Virtuozzo International GmbH. All rights reserved.
 *
 * This file is part of OpenVZ. OpenVZ is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * Lesser General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Our contact details: Virtuozzo International GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 *
 * FAKE methods for setting network parameters
 */

#include "rcconf.h"
#include "exec.h"
#include "../netinfo.h"
#include "../namelist.h"
#include "../common.h"
#include "../options.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

int set_ip(struct netinfo *if_it, struct nettool_mac *params)
{
	VARUNUSED(if_it);
	VARUNUSED(params);
	return -ENOENT;
}

int set_dns(struct netinfo *if_it, struct nettool_mac *params)
{
	VARUNUSED(if_it);
	VARUNUSED(params);
	return -ENOENT;
}

int set_search_domain(struct nettool_mac *params)
{
	VARUNUSED(params);
	return -ENOENT;
}

int set_hostname(struct nettool_mac *params)
{
	char cmd[PATH_MAX + 1];
	size_t len;
	int res;

	if (params->value == NULL)
		return 0;

	/* strip trailing dots */
	len = strlen(params->value);
	while (len > 0 && params->value[len-1] == '.')
		params->value[--len] = 0;

	if (snprintf(cmd, PATH_MAX, "hostname %s", params->value) >= PATH_MAX) {
		werror("ERROR: Command line for execution set_dns.sh is too long");
		return -1;
	}

	res = rcconf_save_fields("hostname", params->value, NULL);
	if (res != 0)
		return res;

	return run_cmd(cmd);
}

int set_gateway(struct netinfo *if_it, struct nettool_mac *params)
{
	VARUNUSED(if_it);
	VARUNUSED(params);
	return -ENOENT;
}

int set_route(struct netinfo *if_it, struct nettool_mac *params)
{
	VARUNUSED(if_it);
	VARUNUSED(params);
	return -ENOENT;
}

int set_dhcp(struct netinfo *if_it, struct nettool_mac *params)
{
	char key_v4[NAME_LENGTH + 10], key_v6[NAME_LENGTH + 15];
	const char *val_v4, *val_v6;
	const char cmd[] = "service dhclient restart ";
	char cmd_buf[sizeof(cmd) + NAME_LENGTH];
	int res;

	VARUNUSED(params);

	snprintf(key_v4, sizeof(key_v4), "ifconfig_%s", if_it->name);
	snprintf(key_v6, sizeof(key_v6), "ifconfig_%s_ipv6", if_it->name);

	val_v4 = (strchr(params->value, '4')) ? "DHCP" : NULL;
	val_v6 = (strchr(params->value, '6')) ? "DHCP" : NULL;

	res = rcconf_save_fields(key_v4, val_v4, key_v6, val_v6, "dhcpd_enable", "YES", NULL);
	if (res != 0)
		return res;

	snprintf(cmd_buf, sizeof(cmd_buf), "%s%s", cmd, if_it->name);

	res = run_cmd(cmd_buf);

	if_it->configured_with_dhcp = (val_v4) ? 1 : 0;
	if_it->configured_with_dhcpv6 = (val_v6) ? 1 : 0;

	return res;
}

int clean(struct netinfo *if_it)
{
	VARUNUSED(if_it);
	return 0;
}

int enable_adapter(struct netinfo *if_it, int enable)
{
	VARUNUSED(if_it);
	VARUNUSED(enable);
	return 0;
}

int restart_guest_network()
{
	return -ENOENT;
}
