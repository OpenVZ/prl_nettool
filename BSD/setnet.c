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

#include "../netinfo.h"
#include "../namelist.h"
#include "../common.h"
#include "../options.h"

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
	VARUNUSED(params);
	return -ENOENT;
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
	VARUNUSED(if_it);
	VARUNUSED(params);
	return -ENOENT;
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
