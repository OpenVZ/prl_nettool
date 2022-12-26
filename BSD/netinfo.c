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
	read_dns(netinfo_head);
	return 0;
}
