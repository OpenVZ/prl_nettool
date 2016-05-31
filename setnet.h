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
 * definition of methods for setting network parameters
 */

#ifndef __SETNET_H__
#define __SETNET_H__


#include "common.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "netinfo.h"
#include "namelist.h"
#include "options.h"

#ifdef _MAC_
int OpenEdit();

int SavePrefs();
#endif

#ifdef _WIN_
int restore_adapter_state();
#endif

int set_ip(struct netinfo *if_it, struct nettool_mac *params);

int set_dns(struct netinfo *if_it, struct nettool_mac *params);

int set_search_domain(struct nettool_mac *params);

int set_hostname(struct nettool_mac *params);

int set_gateway(struct netinfo *if_it, struct nettool_mac *params);

int set_route(struct netinfo *if_it, struct nettool_mac *params);

int set_dhcp(struct netinfo *if_it, struct nettool_mac *params);

/* clean IPs, MASK, gateway if adapter is configured for dhcp */
int clean(struct netinfo *if_it);

int enable_adapter(struct netinfo *if_it, int enable);

int enable_disabled_adapter(const char *mac);

int restart_guest_network();

#endif
