/*
 * Copyright (c) 2015-2017, Parallels International GmbH
 * Copyright (c) 2017-2019 Virtuozzo International GmbH. All rights reserved.
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
 * common methods for getting network parameters
 */

#include "common.h"
#include "netinfo.h"
#include "namelist.h"

#include <unistd.h>

#ifdef _WIN_
#define sleep(s) Sleep(1000*(s))
#endif

void  netinfo_add(struct netinfo *if_info, struct netinfo **netinfo_head)
{
	struct netinfo *it ;
	if (if_info == NULL || netinfo_head == NULL)
		return;
	it = *netinfo_head ;
	*netinfo_head = if_info ;
	if_info->next = it;
}


struct netinfo *netinfo_new(void)
{
	struct netinfo *it = (struct netinfo *) malloc(sizeof(struct netinfo));
	if (it == NULL) {
		error(errno, "can't allocate memory for interface_info");
		return NULL;
	}

	memset(it, 0, sizeof(struct netinfo));

	return it;
}

struct netinfo *netinfo_get_first(struct netinfo **netinfo_head)
{
	struct netinfo *it = *netinfo_head;
	if (it == NULL) {
		it = netinfo_new();
		netinfo_add(it, netinfo_head);
	}
	return it;
}

struct netinfo *netinfo_search_mac(struct netinfo **netinfo_head, const char *mac)
{
	struct netinfo *it = *netinfo_head;
	if (mac == NULL)
		return NULL;
	while (it != NULL){
		if (!strcmp(it->mac, mac))
			return it;
		it = it->next;
	}
	return NULL;
}

struct netinfo *netinfo_search_name(struct netinfo **netinfo_head, const char *name)
{
	struct netinfo *it = *netinfo_head;
	if (name == NULL)
		return NULL;
	while (it != NULL){
		if (!strcasecmp(it->name, name))
			return it;
		it = it->next;
	}
	return NULL;
}

struct netinfo *netinfo_search_idx(struct netinfo **netinfo_head, int idx)
{
	struct netinfo *it = *netinfo_head;
	while (it != NULL){
		if (it->idx == idx)
			return it;
		it = it->next;
	}
	return NULL;
}

void netinfo_free(struct netinfo *it)
{
	if (it == NULL)
		return;
	namelist_clean(&it->ip);
	namelist_clean(&it->search);
	namelist_clean(&it->dns);
	namelist_clean(&it->ip_link);
	namelist_clean(&it->gateway);
	free(it);
}

void netinfo_clean(struct netinfo **netinfo_head)
{
	struct netinfo *it = *netinfo_head, *next;

	while (it != NULL) {
		next = it->next;
		netinfo_free(it);
		it = next;
	}
	*netinfo_head = NULL;
}

const char *mac_to_str(unsigned char *addr, size_t alen,
					char *buf, size_t blen)
{
	unsigned int i;

	if (alen == 0){
		error(0, "empty MAC address");
		return NULL;
	}

	if ((alen - 1) * 3 + 2 + 1 > blen){
		error(0, "buffer for MAC address is too small: %d", blen);
		return NULL;
	}

	buf[0] = '\0';

	for (i = 0; i < alen; i++) {
		int len = strlen(buf);
		if (len == 0)
			snprintf(buf, blen-len, "%02x", addr[i]);
		else
			snprintf(buf+len, blen-len, ":%02x", addr[i]);
	}
	return buf;
}


int is_ipv6(const char *ip)
{
	if (strchr(ip, ':') != NULL)
		return 1;

	return 0;
}


void parse_route(char *value, struct route *route)
{
	if (!value)
		return;

	char *gw = strchr(value, '=');
	if (gw != NULL)
		*gw++ = '\0';
	route->ip = strdup(value);

	if (!gw)
		return;
	char *metric = strchr(gw, 'm');
	if (metric != NULL)
		*metric++ = '\0';
	route->gw = strdup(gw);

	if (!metric)
		return;
	route->metric = strdup(metric);
}


void clear_route(struct route *route)
{
	free(route->ip);
	route->ip = NULL;
	free(route->gw);
	route->gw = NULL;
	free(route->metric);
	route->metric = NULL;
}


int split_ip_mask(const char *ip_mask, char* ip, char *mask)
{
	char tmp[100];
	int len ;
	char *pos;

	strcpy(tmp, ip_mask);
	len = strlen(tmp);

	if (len < (4)) //strlen("::/0")
		goto wrong;

	pos = strchr(tmp, '/');

	if (pos == NULL)
		goto wrong;

	*pos = '\0';

	if (!is_ipv6(ip_mask))
	{
		if (strlen(tmp) < 7 || strlen(tmp) > IP_LENGTH)
				goto wrong;

		strcpy(ip, tmp);

		if (strlen(pos+1) < 7 || strlen(pos+1) > IP_LENGTH)
			goto wrong;

		strcpy(mask, pos+1);
	}else
	{
		int imask = 0;

		if (strlen(pos+1) > 3)
			goto wrong;

		imask = atoi(pos+1);
		if (imask < 0 || imask > 128)
			goto wrong;

		strcpy(mask, pos+1);
		strcpy(ip, tmp);
	}

	return 0;
wrong:
	error(0, "wrong IP/MASK '%s'", ip_mask);
	return -1;
}

void wait_for_start(const struct namelist *adapters)
{
	int enable_count;
	struct netinfo *netinfo_head = NULL;

	for(enable_count=0; enable_count < 60; enable_count++) // wait up to 5 minute
	{
		int all_enabled = 1;

		if (enable_count)
			sleep(5);

		//reload information in system
		int rc = get_device_list(&netinfo_head);
		switch (rc)
		{
#ifdef _WIN_
			// Wait for non-permanent errors to disappear:
			//  ERROR_NO_DATA from GetAdaptersInfo()
			//	network device list is not available yet
			//  ERROR_FILE_NOT_FOUND from getNetInterfaceName()
			//	interface is not yet completed pnp configuration
		case ERROR_NO_DATA:
		case ERROR_FILE_NOT_FOUND:
			continue;
#endif
		case 0:
			break;
		default:
			error(rc, "get_device_list failed: %d", rc);
			return;
		}

		//check enabled adapter
		const struct namelist *it = adapters;
		while(it != NULL)
		{
			if (netinfo_search_mac(&netinfo_head, it->name) == NULL
				&& netinfo_search_name(&netinfo_head, it->name) == NULL
				&& netinfo_search_idx(&netinfo_head, atoi(it->name)) == NULL)
			{
				//not found -> not started
				all_enabled = 0;
				break;
			}

			it=it->next;
		}

		netinfo_clean(&netinfo_head);

		if (all_enabled) {
			debug("Adapters enabled during %d seconds", enable_count);
			return;
		}
	}
}

