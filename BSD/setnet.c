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
#include "rcconf_sublist.h"
#include "exec.h"
#include "../netinfo.h"
#include "../namelist.h"
#include "../common.h"
#include "../options.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <arpa/inet.h>

int set_ip(struct netinfo *if_it, struct nettool_mac *params)
{
	const char key_tmpl[] = "ifconfig_%s";
	const char val_tmpl[] = "inet %s";
	const char cmd_tmpl[] = "ifconfig %s inet %s";

	char key[sizeof(key_tmpl) + NAME_LENGTH];
	char buf[sizeof(cmd_tmpl) + NAME_LENGTH + INET6_ADDRSTRLEN];
	int res;

	if (snprintf(key, sizeof(key), key_tmpl, if_it->name) >= sizeof(key))
		return -EINVAL;

	if (snprintf(buf, sizeof(buf), val_tmpl, params->value) >= sizeof(buf))
		return -EINVAL;

	res = rcconf_save_items(key, buf, NULL);
	if (res != 0)
		return res;

	res = snprintf(buf, sizeof(buf), cmd_tmpl, if_it->name, params->value);
	if (res>= sizeof(buf))
		return -EINVAL;

	if_it->configured_with_dhcp = 0;

	return run_cmd(buf);
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

	res = rcconf_save_items("hostname", params->value, NULL);
	if (res != 0)
		return res;

	return run_cmd(cmd);
}


int set_gateway(struct netinfo *if_it, struct nettool_mac *params)
{
	struct namelist *gws = NULL, *gw;
	const char *key, *val;
	char cmd[PATH_MAX+1];
	int res = 0;

	if (if_it->configured_with_dhcp && if_it->configured_with_dhcpv6) {
		error(0, "WARNING: configured with dhcp. don't change");
		return 0;
	}

	namelist_split(&gws, params->value);
	gw = gws;
	while(gw) {
		key = NULL;
		val = NULL;

		if (is_ipv6(gw->name) || is_removev6(gw->name)) {
			snprintf(cmd, PATH_MAX, "route -6 del default; ");

			if (!is_removev6(gw->name)) {
				snprintf(cmd, PATH_MAX, "%s route -6 add default %s -ifp %s",
						 cmd, gw->name, if_it->name);
				val = gw->name;
			}
			key = "ipv6_defaultrouter";
		} else {
			snprintf(cmd, PATH_MAX, "route del default; ");

			if (!is_remove(gw->name)) {
				snprintf(cmd, PATH_MAX, "%s route add default %s -ifp %s",
						 cmd, gw->name, if_it->name);
				val = gw->name;
			}
			key = "defaultrouter";
		}
		res = run_cmd(cmd);
		if (res != 0)
			return res;

		res = rcconf_save_items(key, val, NULL);
		if (res != 0)
			return res;

		gw = gw->next;
	}
	namelist_clean(&gws);

	return res;
}

int set_route(struct netinfo *if_it, struct nettool_mac *params)
{
	struct namelist *gws = NULL, *gw;
	struct rcconf cfg;
	struct rcconf_sublist sublist;
	const char *ipv = NULL, *gw_ip, *if_arg;
	char cmd[PATH_MAX+1];
	int res = 0;

	if (if_it->configured_with_dhcp && if_it->configured_with_dhcpv6) {
		error(0, "WARNING: configured with dhcp. don't change");
		return 0;
	}

	rcconf_init(&cfg);

	res = rcconf_load(&cfg);
	if (res != 0) {
		return res;
	}

	rcconf_sublist_init(&sublist, "static_routes", "route_", "prl");
	rcconf_sublist_load(&sublist, &cfg);

	namelist_split(&gws, params->value);
	gw = gws;
	while(gw) {
		struct route route = {NULL, NULL, NULL};

		parse_route(gw->name, &route);

		ipv = (is_ipv6(gw->name)) ? "-6" : "-4";
		if (route.gw) {
			gw_ip = route.gw;
			if_arg = "-ifp";
		} else {
			gw_ip = "";
			if_arg = "-interface";
		}

		snprintf(cmd, PATH_MAX, "route del %s %s;"
				"route add %s %s %s %s %s",
				ipv, route.ip,
				ipv, route.ip, gw_ip, if_arg, if_it->name);
		res = run_cmd(cmd);
		if (res != 0) {
			goto end;
		}

		snprintf(cmd, PATH_MAX, "%s %s %s %s %s",
				 ipv, route.ip, gw_ip, if_arg, if_it->name);

		rcconf_sublist_set(&sublist, cmd);

		clear_route(&route);
		gw = gw->next;
	}
	namelist_clean(&gws);

	rcconf_sublist_save(&sublist, &cfg);
	rcconf_save(&cfg);

end:
	rcconf_sublist_free(&sublist);
	rcconf_free(&cfg);

	return res;
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

	res = rcconf_save_items(key_v4, val_v4, key_v6, val_v6, "dhcpd_enable", "YES", NULL);
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
