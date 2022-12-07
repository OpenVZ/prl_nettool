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
 * Parse command line options
 */

#include "common.h"
#include "options.h"
#include "namelist.h"


static void usage(int err_code)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "prl_nettool get \n"
							"   [ --all | --gateway [<MAC>] | --dns [<MAC>] |" \
							" --dhcp [<MAC>] | --ip [<MAC>] | --search-domain ] ... \n");
	fprintf(stderr, "prl_nettool set <command> ...\n" \
							" available commands:\n" \
							"   --dhcp <MAC>              - switch to DHCP \n" \
							"   --dhcpv6 <MAC>            - switch IPv6 to DHCP \n" \
							"   --gateway <MAC> <IP>      - set default gateway \n" \
							"   --dns <MAC> <IPs>         - set dns servers\n" \
							"   --ip <MAC> <IP/MASK>      - set ip addresses to adapter\n" \
							"                              it automatically switch to static\n" \
							"                              if dhcp was enabled\n" \
							"   --search-domain <domains> - set search domain \n" \
							"   --hostname <hostname>     - set hostname\n" \
							"   --route <MAC> <IP1>[/MASK][=<IP2>][m<metric>] - set route to <IP1> via <IP2> dev <MAC> metric <metric>\n" );
#ifdef _MAC_
	fprintf(stderr, "prl_nettool clean \n");
#endif
#ifdef _LIN_
	fprintf(stderr, "prl_nettool restart \n");
#endif


	exit(err_code);
}

static void version(void)
{
	fprintf(stderr, "%s %s\n", TOOL_NAME, VERSION);
	exit(0);
}

struct nettool_options net_opts;

void set_empty_options()
{
	net_opts.command_flags = 0;
	net_opts.verbose = 0;
	net_opts.macs = NULL;
	net_opts.debug = 0;
	net_opts.action = 0;
	net_opts.compare = 0;
}

void set_option(unsigned int opt)
{
	net_opts.command_flags  = net_opts.command_flags | opt;
}

void add_opt_mac(unsigned int opt, const char *mac)
{
	struct nettool_mac *mac_it = NULL;
	if (mac == NULL) //wrong usage
		return;

	mac_it = net_opts.macs;
	while(mac_it != NULL)
	{
		if (mac_it->mac != NULL && !strcmp(mac, mac_it->mac))
		{
			mac_it->type = mac_it->type | opt;
			return;
		}
		mac_it = mac_it->next;
	}

	mac_it = (struct nettool_mac *) malloc(sizeof(struct nettool_mac));
	if (mac_it == NULL) { //error
		error(errno, "Can't allocate memory for nettool_mac_t");
		return;
	}
	mac_it->next = net_opts.macs;
	mac_it->type = opt;
	mac_it->mac = strdup(mac);
	if (mac_it->mac == NULL) { //error
		free((void *)mac_it);
		error(errno, "Can't allocate memory for mac address");
		return;
	}
	net_opts.macs = mac_it;
}

void add_set_opt(unsigned int opt, const char *mac, const char *value)
{
	struct nettool_mac *mac_it = NULL;

	mac_it = (struct nettool_mac *) malloc(sizeof(struct nettool_mac));
	if (mac_it == NULL) { //error
		error(errno, "Can't allocate memory for nettool_mac_t");
		return;
	}
	mac_it->next = net_opts.macs;
	mac_it->type = opt;
	if (mac != NULL) {
		mac_it->mac = strdup(mac);
		if (mac_it->mac == NULL) { //error
			free((void *)mac_it);
			error(errno, "Can't allocate memory for mac address");
			return;
		}
	}else
		mac_it->mac = NULL;

	if (value != NULL) {
		mac_it->value = strdup(value);
		if (mac_it->value == NULL) { //error
			if (mac_it->mac != NULL)
				free(mac_it->mac);
			free((void *)mac_it);
			error(errno, "Can't allocate memory for value");
			return;
		}
	}
	else
		mac_it->value = NULL;

	net_opts.macs = mac_it;
}

int is_opt_set(unsigned int opt)
{
	return (net_opts.command_flags & opt);
}

void clean_opt_mac(unsigned int opts)
{
	struct nettool_mac *mac_it = net_opts.macs;
	while(mac_it != NULL)
	{
		if (mac_it->type & opts) {
			CLEAN_OPT(mac_it->type, opts);
		}

		mac_it = mac_it->next;
	}
}

int count_opt_mac(unsigned int opts)
{
	int count  = 0;

	struct nettool_mac *mac_it = net_opts.macs;
	while(mac_it != NULL)
	{
		if (mac_it->type & opts)
			count ++;

		mac_it = mac_it->next;
	}
	return count;
}

/*search option with same type and mac
return 1 - found
       0 - not found
*/
int search_opt_mac(const char *mac, unsigned int opts)
{
	struct nettool_mac *mac_it = net_opts.macs;
	while(mac_it != NULL)
	{
		if ((mac_it->type & opts)
			&& mac_it->mac != NULL
			&& !strcmp(mac, mac_it->mac))
		{
			return 1;
		}

		mac_it = mac_it->next;
	}
	return 0;
}

struct nettool_mac *get_opt_mac(const char *mac, unsigned int opts)
{
	struct nettool_mac *mac_it = net_opts.macs;
	while(mac_it != NULL)
	{
		if (mac_it->type & opts) {
			if (mac == NULL)
				return mac_it;

			if (mac_it->mac != NULL &&
				!strcmp(mac, mac_it->mac))
				return mac_it;
		}

		mac_it = mac_it->next;
	}
	return NULL;
}

int is_ipv6_value(char *str)
{
	struct namelist *values = NULL;
	struct namelist *value;

	if (str == NULL || strlen(str) == 0)
		return 0;

	namelist_split(&values, str);
	value = values;
	while(value)
	{
		if (is_ipv6(value->name) || is_removev6(value->name))
		{
			error(0, "Value '%s' is ipv6. Not supported by OS.", value->name);
			namelist_clean(&values);
			return 1;
		}

		value = value->next;
	}
	namelist_clean(&values);

	return 0;
}

char *clean_ipv6_value(char *str)
{
	struct namelist *values = NULL;
	struct namelist *value;
	struct namelist *clean_values = NULL;
	char clean_str[2048];

	if (str == NULL || strlen(str) == 0)
		return 0;

	namelist_split(&values, str);
	value = values;
	while(value)
	{
		if ((!is_ipv6(value->name)) && (!is_removev6(value->name)))
		{
			namelist_add(value->name, &clean_values);
		}

		value = value->next;
	}
	namelist_clean(&values);

	print_namelist_to_str(&clean_values, clean_str, sizeof(clean_str));
	namelist_clean(&clean_values);

	return strdup(clean_str);
}


void parse_options(char *argv[])
{
	unsigned int opt = 0;
	unsigned int argn = 0;
	char *value;
	int is_support_ipv6 = is_ipv6_supported();
	set_empty_options();

	argv++;
	argn = 1;
	while (*argv != NULL) {
		char *command;

		if (!strcmp(*argv, "--all")) {
			set_option( NET_OPT_ALL );
			clean_opt_mac( NET_OPT_ALL );
			return;
		}
		if (!strcmp(*argv, "-V") || !strcmp(*argv, "--version"))
		{
			version();
			return;
		}
		if (!strcmp(*argv, "-h") || !strcmp(*argv, "--help"))
		{
			usage(0);
			return;
		}

		opt = 0;

		command = *argv;

		if (argn == 1 && !strcmp(command, "get"))
		{
			net_opts.action = GET;
		}else if (argn == 1 && !strcmp(command, "set"))
		{
			net_opts.action = SET;
		}else if (argn == 1 && !strcmp(command, "clean"))
		{
			net_opts.action = CLEAN;
		}else if (argn == 1 && !strcmp(command, "restart"))
		{
			net_opts.action = RESTART;
		}else if (!strcmp(command, "-v") || !strcmp(command, "--verbose"))
		{
			net_opts.verbose = 1;
		}else if(!strcmp(command, "-d") || !strcmp(command, "--debug") )
		{
			net_opts.debug = 1;
		}else if (!strcmp(command, "--gateway"))
		{
			opt = NET_OPT_GATEWAY ;
		}
		else if (!strcmp(command, "--dns"))
		{
			opt = NET_OPT_DNS ;
		}
		else if (!strcmp(command, "--search-domain"))
		{
			opt = NET_OPT_SEARCH ;
		}
		else if (!strcmp(command, "--dhcp"))
		{
			opt = NET_OPT_DHCP ;
		}
		else if (!strcmp(command, "--dhcpv6"))
		{
			opt = NET_OPT_DHCPV6 ;
		}
		else if (!strcmp(command, "--ip"))
		{
			opt = NET_OPT_IP ;
		}
		else if (!strcmp(command, "--default-route"))
		{
			opt = NET_OPT_GATEWAY ;
		}
		else if (!strcmp(command, "--route"))
		{
			opt = NET_OPT_ROUTE ;
		}
		else if (!strcmp(command, "--hostname"))
		{
			opt = NET_OPT_HOSTNAME;
		}
		else if (!strcmp(command, "--compare"))
		{
			net_opts.compare = 1;
		}
		else{
			error(0, "Unknown argument '%s'", command);
			usage(1);
			return;
		}

		argv ++;
		argn ++;
		if (opt != 0 && net_opts.action == GET){ //to get parameters
			if (opt & NET_OPT_GETBYMAC) {
				if (*argv != NULL && *argv[0] != '-')
				{
					add_opt_mac( opt, *argv );
					argv ++;
					argn ++;
				}else{
					set_option( opt );
				}
			}
			if (opt & NET_OPT_GETNOTMAC) {
				set_option( opt );
			}
		}else if (opt != 0 && net_opts.action == SET) {//to set parameters
			if (*argv == NULL || *argv[0] == '-' ) {
				error(0, "MAC or value should be specified for '%s'", command);
				usage(1);
				return;
			}

			if (opt & NET_OPT_GETBYMAC) {
				char *mac = *argv;
				argv ++;
				argn ++;

				//for dhcp needed MAC address only
				if (opt == NET_OPT_DHCP || opt == NET_OPT_DHCPV6)
				{
					struct nettool_mac *opt_mac = get_opt_mac(mac, NET_OPT_DHCP);
					if (opt_mac && opt_mac->value &&
						(!strcmp(opt_mac->value, "4") || !strcmp(opt_mac->value, "6")))
					{
						free(opt_mac->value);
						opt_mac->value = strdup("4 6");
					}
					else if (opt_mac)
					{
						error(0, "Internal error. Wrong --dhcp or --dhcpv6 option");
						return;
					}
					else if (opt == NET_OPT_DHCP)
					{
						add_set_opt(opt, mac, "4");
					}
					else if (opt == NET_OPT_DHCPV6)
					{
						if (!is_support_ipv6)
						{
							error(0, "IPv6 is not supported on current OS");
							exit(3);
						}

						add_set_opt(NET_OPT_DHCP, mac, "6");
					}
				}
				else
				{
					//should be MAC and value
					if (*argv == NULL)
					{
						error(0, "Value should be specified for '%s'", command);
						usage(1);
						return;
					}

					value = *argv;
					if (!is_support_ipv6 && is_ipv6_value(*argv))
 					{
						error(0, "IPv6 is not supported on current OS");
						exit(3);
					}

					add_set_opt(opt, mac, value);
					if (value != NULL && value != (*argv))
						free(value);
					argv ++;
					argn ++;
				}
			}else if (opt & NET_OPT_GETNOTMAC){
				//search domain without mac. only value
				add_set_opt(opt, NULL, *argv);
				argv ++;
				argn ++;
			}
		}
	}

	if (net_opts.action == GET)
	{
		if (net_opts.command_flags == 0 && count_opt_mac(NET_OPT_GETBYMAC) == 0)
			set_option( NET_OPT_ALL );
	}
	else if (net_opts.action == SET)
	{
		if (count_opt_mac(NET_OPT_GETBYMAC) == 0 && count_opt_mac(NET_OPT_GETNOTMAC) == 0)
		{
			error(0, "Nothing to set");
			usage(0);
			return;
		}
	}

	value = getenv("PRL_NETTOOLS_OPT");
	if (value && strcasestr(value, "--compare"))
		net_opts.compare = 1;
}
