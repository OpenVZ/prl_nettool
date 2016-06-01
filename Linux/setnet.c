/*
 * Copyright (c) 2015 Parallels IP Holdings GmbH
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
 * Our contact details: Parallels IP Holdings GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 *
 * FAKE methods for setting network parameters
 */

#include "../common.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include "../netinfo.h"
#include "../namelist.h"
#include "../options.h"
#include "exec.h"
#include "detection.h"

extern int os_vendor;
extern char * os_script_prefix;

int remove_ipv6(struct netinfo *if_it)
{
	char cmd[PATH_MAX+1];

	if (snprintf(cmd, PATH_MAX,
				"ip -6 addr flush dev %s scope global ", if_it->name) >= PATH_MAX)
	{
		werror("ERROR: Command line for execution 'ip -6 addr' is too long");
		return -1;
	}

	run_cmd(cmd);

	return 0;
}

int set_ip(struct netinfo *if_it, struct nettool_mac *params){
	char cmd[PATH_MAX+1];
	int rc;
	char opts[100] = {'\0'};

	if (!os_script_prefix) {
		werror("Current distribution is not supported");
		return 0; //TODO: need to support other distribs
	}

	struct nettool_mac *mac_it = get_opt_mac(if_it->mac, NET_OPT_DHCP);
	if (mac_it != NULL && mac_it->value != NULL)
	{
		if (strchr(mac_it->value, '6'))
			strcat(opts, "dhcpv6");
		else
			strcat(opts, "nodhcpv6");

		strcat(opts, " ");

		if (strchr(mac_it->value, '4'))
			strcat(opts, "dhcp");
		else
			strcat(opts, "nodhcp");
	}

	if (snprintf(cmd, PATH_MAX, SCRIPT_DIR "/%s-set_ip.sh \"%s\" \"%s\" \"%s\" \"%s\"",
			os_script_prefix,
			if_it->name, if_it->mac, params->value, opts) >= PATH_MAX)
	{
		werror("ERROR: Command line for execution %s-set_ip.sh is too long", os_script_prefix);
		return -1;
	}

	if_it->configured_with_dhcp = 0;

	rc = run_cmd(cmd);

	return rc;
}

int set_dns(struct netinfo *if_it, struct nettool_mac *params){
	char cmd[PATH_MAX+1];
	int rc;

	(void)if_it; /* to make compiler happy */

	if (params->value == NULL)
		return 0;

	/* We had better use dhclient-$IF.conf here,
	 * but it brings a lot of problems when e.g. iface is bridged
	 * so just leave the old (global, not per-interface) behaviour.
	 * */
	if (snprintf(cmd, PATH_MAX, SCRIPT_DIR "/set_dns.sh \"%s\" \"\" \"\" \"%s\"",
			params->value, (os_script_prefix != NULL) ?  os_script_prefix : "") >= PATH_MAX)
	{
		werror("ERROR: Command line for execution set_dns.sh is too long");
		return -1;
	}
	rc = run_cmd(cmd);

	return rc;
}

int set_search_domain(struct nettool_mac *params) {
	char cmd[PATH_MAX+1];
	int rc;

	if (params->value == NULL)
		return 0;

	if (snprintf(cmd, PATH_MAX, SCRIPT_DIR "/set_dns.sh \"\" \"%s\"",
			params->value) >= PATH_MAX)
	{
		werror("ERROR: Command line for execution set_dns.sh is too long");
		return -1;
	}
	rc = run_cmd(cmd);

	return rc;
}

int set_hostname(struct nettool_mac *params)
{
	char cmd[PATH_MAX+1];
	size_t len;

	if (params->value == NULL)
		return 0;

	/* strip trailing dots */
	len = strlen(params->value);
	while (len > 0 && params->value[len-1] == '.')
		params->value[--len] = 0;

	if (snprintf(cmd, PATH_MAX, SCRIPT_DIR "/set_dns.sh \"\" \"\" \"%s\" \"%s\"",
		params->value, (os_script_prefix != NULL) ?  os_script_prefix : "")
				>= PATH_MAX) {
		werror("ERROR: Command line for execution set_dns.sh is too long");
		return -1;
	}

	return run_cmd(cmd);
}

int set_gateway(struct netinfo *if_it, struct nettool_mac *params) {

	char cmd[PATH_MAX+1];
	int rc = 0;

	if (if_it->configured_with_dhcp && if_it->configured_with_dhcpv6)
	{
		error(0, "WARNING: configured with dhcp. don't change");
		return 0;
	}

	if (os_script_prefix != NULL) { //TODO: need to support other distribs
		if (snprintf(cmd, PATH_MAX, SCRIPT_DIR "/%s-set_gateway.sh \"%s\" \"%s\" \"%s\"",
			os_script_prefix, if_it->name, params->value, if_it->mac) >= PATH_MAX)
		{
			werror("ERROR: Command line for execution %s-set_gateway.sh is too long", os_script_prefix);
			return -1;
		}

		rc = run_cmd(cmd);
	}else{
		struct namelist *gws = NULL, *gw;
		namelist_split(&gws, params->value);
		gw = gws;
		while(gw)
		{
			if (is_ipv6(gw->name) ||
				!strncmp(gw->name, NET_STR_OPT_REMOVEV6, strlen(NET_STR_OPT_REMOVEV6)))
			{
				//ipv6
				snprintf(cmd, PATH_MAX, "route -A inet6 del default; ");

				if (strncmp(gw->name, NET_STR_OPT_REMOVEV6, strlen(NET_STR_OPT_REMOVEV6)))
					snprintf(cmd, PATH_MAX, "%s route -A inet6 add default gw %s %s",
						cmd, gw->name, if_it->name);
			}
			else
			{
				//ipv4
				//use generic route command
				snprintf(cmd, PATH_MAX, "route del default; ");

				if (strncmp(gw->name, NET_STR_OPT_REMOVE, strlen(NET_STR_OPT_REMOVE)))
					snprintf(cmd, PATH_MAX, "%s route add default gw %s %s",
						cmd, gw->name, if_it->name);
			}
			rc = run_cmd(cmd);

			gw = gw->next;
		}
		namelist_clean(&gws);
	}

	return rc;
}

int set_route(struct netinfo *if_it, struct nettool_mac *params)
{

	char cmd[PATH_MAX+1];
	char ip_cmd[50];
	int rc = 0;

	if (if_it->configured_with_dhcp && if_it->configured_with_dhcpv6)
	{
		error(0, "WARNING: configured with dhcp. don't change");
		return 0;
	}

	if (os_script_prefix != NULL) {
		if (snprintf(cmd, PATH_MAX, SCRIPT_DIR "/%s-set_route.sh \"%s\" \"%s\" \"%s\"",
			os_script_prefix, if_it->name, params->value,
			if_it->mac) >= PATH_MAX)
		{
			werror("ERROR: Command line for execution %s-set_route.sh is too long", os_script_prefix);
			return -1;
		}

		rc = run_cmd(cmd);
	}else{
		struct namelist *gws = NULL, *gw;
		namelist_split(&gws, params->value);
		gw = gws;
		while(gw)
		{
			if (is_ipv6(gw->name))
				snprintf(ip_cmd, sizeof(ip_cmd), "ip -6");
			else
				snprintf(ip_cmd, sizeof(ip_cmd), "ip");

			struct route route = {NULL, NULL, NULL};
			parse_route(gw->name, &route);

			snprintf(cmd, PATH_MAX, " %s route del %s %s %s dev %s;"
					" %s route add %s %s %s dev %s %s %s",
					ip_cmd, route.ip, route.gw ? "via" : "", route.gw, if_it->name,
					ip_cmd, route.ip, route.gw ? "via" : "", route.gw, if_it->name,
					route.metric ? "metric" : "", route.metric);
			rc = run_cmd(cmd);

			clear_route(&route);
			gw = gw->next;
		}
		namelist_clean(&gws);
	}

	return rc;
}

int set_dhcp(struct netinfo *if_it, struct nettool_mac *params) {
	int rc = 0;

	if (!os_script_prefix)
	{
		werror("Distribution not supported");
		return -1;
	}

	//switch to dynamic
	char cmd[PATH_MAX+1];

	struct nettool_mac *mac_it = get_opt_mac(if_it->mac, NET_OPT_IP);
	if (strchr(params->value, '6') == NULL && mac_it == NULL)
	{
		//ipv6 not specified at all. remove all ips
		remove_ipv6(if_it);
	}

	if (snprintf(cmd, PATH_MAX, SCRIPT_DIR "/%s-set_dhcp.sh \"%s\" \"%s\" \"%s\" ",
			os_script_prefix, if_it->name, if_it->mac, params->value ) >= PATH_MAX)
	{
		werror("ERROR: Command line for execution %s-set_dhcp.sh is too long", os_script_prefix);
		return -1;
	}
	rc = run_cmd(cmd);

	if (strchr(params->value, '4'))
		if_it->configured_with_dhcp = 1;
	if (strchr(params->value, '6'))
		if_it->configured_with_dhcpv6 = 1;


	VARUNUSED(params);

	return rc;
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
	char cmd[PATH_MAX+1];

	if (!os_script_prefix)
	{
		werror("Distribution not supported");
		return -1;
	}

	if (snprintf(cmd, PATH_MAX, SCRIPT_DIR "/%s-restart.sh",
			os_script_prefix) >= PATH_MAX)
	{
		werror("ERROR: Command line for execution %s-restart.sh is too long", os_script_prefix);
		return -1;
	}
	return run_cmd(cmd);
}
