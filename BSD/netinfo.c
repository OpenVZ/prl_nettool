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
 */

#include "rcprl.h"
#include "../netinfo.h"
#include "../namelist.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/ethernet.h>

static struct netinfo *make_netinfo(struct ifaddrs *ifaddrs)
{
	struct sockaddr_dl *sdl;
	struct netinfo *if_info;

	if (ifaddrs->ifa_addr->sa_family != AF_LINK)
		return NULL;

	sdl = (struct sockaddr_dl *) ifaddrs->ifa_addr;

	switch (sdl->sdl_type) {
		case IFT_ETHER:
		case IFT_XETHER:
		case IFT_PPP:
			break;
		default:
			return NULL;
	}

	if_info = netinfo_new();
	if (if_info == NULL)
		return NULL;

	mac_to_str((unsigned char *)LLADDR(sdl), ETHER_ADDR_LEN,
			   if_info->mac, MAC_LENGTH + 1);

	strlcpy(if_info->name, ifaddrs->ifa_name, NAME_LENGTH);

	return if_info;
}

static int get_ifaces(struct netinfo **netinfo_head)
{
	struct ifaddrs *ifaddrs;
	struct netinfo *if_info;
	int res;

	res = getifaddrs(&ifaddrs);
	if (res != 0) {
		return res;
	}

	while (ifaddrs) {
		if_info = make_netinfo(ifaddrs);
		if (if_info != NULL)
			netinfo_add(if_info, netinfo_head);
		ifaddrs = ifaddrs->ifa_next;
	}
	return 0;
}

static int check_dhcp_item(struct rcconf_item *item)
{
	const char *vals[] = {
		"DHCP",
		"SYNCDHCP",
		"NOSYNCDHCP",
		NULL
	};
	const char **p;

	if ((!item) || (!item->val))
		return 0;

	for (p = vals; *p; p++) {
		if (strcmp(item->val, *p) == 0)
			return 1;
	}

	return 0;
}

static int check_dhcp(struct rcconf *rcmain, const char *key)
{
	struct rcconf_item *item;

	item = rcprl_get_item(key);
	if (check_dhcp_item(item))
		return 1;

	item = rcconf_get_item(rcmain, key);
	return check_dhcp_item(item);
}

void read_dhcp(struct netinfo **netinfo_head)
{
	const char *tmpl_v4 = "ifconfig_%s";
	const char *tmpl_v6 = "ifconfig_%s_ipv6";
	char key_v4[NAME_LENGTH + sizeof(tmpl_v4)];
	char key_v6[NAME_LENGTH + sizeof(tmpl_v6)];
	struct netinfo *info;
	struct rcconf rcmain;

	rcprl_init();
	if (rcprl_load() != 0)
		return;

	rcconf_init(&rcmain);
	if (rcconf_load(&rcmain, RC_PATH) != 0)
		goto err;

	info = netinfo_get_first(netinfo_head);
	while(info && strlen(info->mac)) {
		if (snprintf(key_v4, sizeof(key_v4), tmpl_v4, info->name) >= sizeof(key_v4))
			goto err;

		if (snprintf(key_v6, sizeof(key_v6), tmpl_v6, info->name) >= sizeof(key_v6))
			goto err;

		info->configured_with_dhcp = check_dhcp(&rcmain, key_v4);
		info->configured_with_dhcpv6 = check_dhcp(&rcmain, key_v6);

		info = info->next;
	}

err:
	rcconf_free(&rcmain);
	rcprl_free();
}

void read_dns(struct netinfo **netinfo_head)
{
	struct netinfo *ifinfo = NULL;
	char *line = NULL;
	char dns[INET6_ADDRSTRLEN + 1];
	char ifname[NAME_LENGTH + 1];
	size_t len = 0;
	FILE *f;

	f = popen("resolvconf -l", "r");
	if (f == NULL)
		return;

	while (getline(&line, &len, f) >= 0) {
		/* 260 - NAME_LENGTH */
		if (sscanf(line, "# resolv.conf from %260s", ifname) == 1) {
			ifinfo = netinfo_search_name(netinfo_head, ifname);
			continue;
		}

		/* 46 - INET6_ADDRSTRLEN */
		if (sscanf(line, "nameserver %46s", dns) != 1)
			continue;

		if (ifinfo)
			namelist_add(dns, &ifinfo->dns);
	}
	pclose(f);
}

int get_device_list(struct netinfo **netinfo_head)
{
	get_ifaces(netinfo_head);
	read_dhcp(netinfo_head);
	read_dns(netinfo_head);
	return 0;
}
