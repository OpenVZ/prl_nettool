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
 * Parse command line options
 */

#ifndef __OPTIONS_H__
#define __OPTIONS_H__

//#include "Build/Tools.ver"
#define TOOL_NAME	"Parallels Guest Nettool"

#define NET_OPT_ALL		0xFF
#define NET_OPT_GATEWAY		0x01
#define NET_OPT_DNS		0x02
#define NET_OPT_SEARCH		0x04 //search domain
#define NET_OPT_IP		0x08
#define NET_OPT_DHCP		0x10 //DHCP
#define NET_OPT_ROUTE		0x20
#define NET_OPT_DHCPV6		0x40 //DHCP v6
#define NET_OPT_HOSTNAME	0x80

#define NET_OPT_GETBYMAC	(NET_OPT_GATEWAY | NET_OPT_DNS | NET_OPT_DHCP | \
					NET_OPT_DHCPV6 | NET_OPT_IP | NET_OPT_ROUTE)
#define NET_OPT_GETNOTMAC	(NET_OPT_SEARCH | NET_OPT_HOSTNAME)
#define NET_OPT_IPVALUE	(NET_OPT_GATEWAY | NET_OPT_DNS | NET_OPT_IP | NET_OPT_ROUTE)

#define NET_STR_OPT_REMOVE	"remove"
#define NET_STR_OPT_REMOVEV6	"removev6"

#define CLEAN_OPT(mode, clean_bit)  ((mode) &= ~(clean_bit))

enum ACTION	{
	GET = 0,
	SET = 1,
	CLEAN = 2, /*used for MAX OS X. clean configuration if is congured to DHCP */
	RESTART = 3 /*used to restart network inside linux guest*/
};

struct nettool_mac
{
	unsigned int type;
	char * mac;
	struct nettool_mac *next;
	//fields for setting
	char * value; //IP or else
};

struct nettool_options
{
	unsigned int command_flags;
	struct nettool_mac *macs;
	int verbose, debug, compare;
	enum ACTION action;
};



void set_empty_options();

void set_option(unsigned int opt);

void add_opt_mac(unsigned int opt, const char *mac);

int count_opt_mac(unsigned int opts);

/* search option with same type and mac
return 1 - found
       0 - not found
*/
int search_opt_mac(const char *mac, unsigned int opts);

struct nettool_mac *get_opt_mac(const char *mac, unsigned int opts);

void clean_opt_mac(unsigned int opts);

int is_opt_set(unsigned int opt);

void parse_options(char **argv);


#endif // __OPTIONS_H__
