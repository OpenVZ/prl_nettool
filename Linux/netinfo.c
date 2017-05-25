/*
 * Copyright (c) 2015-2017, Parallels International GmbH
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
 * Our contact details: Parallels International GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <arpa/inet.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include "../netinfo.h"
#include "../namelist.h"
#include <libnetlink.h>
#include "../common.h"
#include "detection.h"
#include "exec.h"

struct namelist *bridge_names = NULL;

int os_vendor;
char *os_script_prefix = NULL;

struct nlmsg_list
{
	struct nlmsg_list *next;
	struct nlmsghdr h;
};

static int store_nlmsg(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	struct nlmsg_list **linfo = (struct nlmsg_list**)arg;
	struct nlmsg_list *h;
	struct nlmsg_list **lp;

	VARUNUSED(who);

	h = malloc(n->nlmsg_len+sizeof(void*));
	if (h == NULL) {
		werror("can't allocate memory");
		return -1;
	}

	memcpy(&h->h, n, n->nlmsg_len);
	h->next = NULL;

	for (lp = linfo; *lp; lp = &(*lp)->next) /* NOTHING */;
	*lp = h;

	return 0;
}

static void clean_nlmsg(struct nlmsg_list *list)
{
	struct nlmsg_list *l, *n;

	for (l=list; l; l = n) {
		n = l->next;
		free(l);
	}
}


static int put_gateway(struct nlmsghdr *n, struct netinfo **netinfo_head, int is_ipv6)
{
   	struct rtmsg *r = NLMSG_DATA(n);
	int len;
	struct rtattr * rta_tb[RTA_MAX+1];
	char abuf[256];
	const char *ret;

	if (n->nlmsg_type != RTM_NEWROUTE)
		return 0;

	if (r->rtm_type != RTN_UNICAST)
		return 0;

	if (!is_ipv6)
	{
		if (r->rtm_family != AF_INET)
			return 0;
	}
	else
	{
		if (r->rtm_family != AF_INET6)
			return 0;
	}

	len = n->nlmsg_len - NLMSG_LENGTH(sizeof(*r));
	if (len < 0) {
		werror("netlink message too short to be a routing message");
		return 1;
	}

	parse_rtattr(rta_tb, RTA_MAX, RTM_RTA(r), len);


	if (!rta_tb[RTA_GATEWAY]) //no gateway
		return 0;

	if (rta_tb[RTA_DST] || r->rtm_dst_len) //not default
		return 0;

	ret = inet_ntop(r->rtm_family, RTA_DATA(rta_tb[RTA_GATEWAY]), abuf, sizeof(abuf));

	if (ret)
	{
		struct netinfo *it = netinfo_get_first(netinfo_head);
		while (it != NULL){
			if (namelist_add(abuf, &(it->gateway)) < 0)
				return -1;

			it= it->next;
		}
	}

	return 0;
}


static int put_addrinfo(struct nlmsghdr *n, struct netinfo *if_info)
{
	struct ifaddrmsg *ifa = NLMSG_DATA(n);
	struct rtattr * ifa_tb[IFA_MAX+1];
	int len;
	char ip_buf[256];

	len = n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa));
	if (len < 0)
		return 1;
	parse_rtattr(ifa_tb, IFA_MAX, IFA_RTA(ifa), len);

	if (!ifa_tb[IFA_LOCAL])
		ifa_tb[IFA_LOCAL] = ifa_tb[IFA_ADDRESS];
	if (!ifa_tb[IFA_ADDRESS])
		ifa_tb[IFA_ADDRESS] = ifa_tb[IFA_LOCAL];

	if (ifa_tb[IFA_LOCAL]) {
		inet_ntop(ifa->ifa_family,
                        RTA_DATA(ifa_tb[IFA_LOCAL]), ip_buf, sizeof(ip_buf));

		if (!ifa_tb[IFA_ADDRESS] ||
		    !memcmp(RTA_DATA(ifa_tb[IFA_ADDRESS]),
					RTA_DATA(ifa_tb[IFA_LOCAL]), 4))
		{
			char mask_buf[256];
			char ip_mask[256];

			if (ifa->ifa_family == AF_INET)
			{
				uint32_t mask = 0xFFFFFFFF >> (32 - ifa->ifa_prefixlen);

				inet_ntop(ifa->ifa_family,
        	              		(void *)&mask, mask_buf, sizeof(mask_buf));
			} else {
				sprintf(mask_buf, "%d", ifa->ifa_prefixlen);
			}

			sprintf(ip_mask, "%s/%s", ip_buf, mask_buf);
			if (namelist_add(ip_mask, &(if_info->ip)) < 0)
				return -1;
		}
	}
	return 0;
}

static int is_bridge(const struct dirent *entry)
{
	char path[PATH_MAX];
	struct stat st;

	snprintf(path, PATH_MAX, "/sys/class/net/%s/bridge", entry->d_name);
	if ((stat(path, &st) == 0) && S_ISDIR(st.st_mode))
		return 1;
	return 0;
}

void read_bridge_info()
{
	struct dirent **ifaces;
	int i, num;

	num = scandir("/sys/class/net", &ifaces, is_bridge, alphasort);
	if (num < 0)
		return;

	for (i = 0; i < num; i++) {
		namelist_add(ifaces[i]->d_name, &bridge_names);
		free(ifaces[i]);
	}
	free(ifaces);
}

static struct netinfo *put_linkinfo(struct nlmsghdr *n,
				struct nlmsg_list *ipinfo,
				struct nlmsg_list *ip6info)
{
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct rtattr * tb[IFLA_MAX+1];
	int len;
	char buf[20];
	char *dev_name;
	struct netinfo *if_info = NULL;

	if (n->nlmsg_type != RTM_NEWLINK)
		return 0;

	len = n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0)
		return NULL;

	/* skip loopback */
	if (ifi->ifi_type == ARPHRD_LOOPBACK)
		return 0;

	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);
	if (!tb[IFLA_IFNAME])
		return 0;

	if (!tb[IFLA_ADDRESS])
		return 0;

	if (!(ifi->ifi_type == ARPHRD_ETHER ||
		ifi->ifi_type == ARPHRD_EETHER ||
		ifi->ifi_type == ARPHRD_IEEE802 ||
		ifi->ifi_type == ARPHRD_PPP ||
		ifi->ifi_type == ARPHRD_NETROM))
		return 0;

	if (mac_to_str(RTA_DATA(tb[IFLA_ADDRESS]), RTA_PAYLOAD(tb[IFLA_ADDRESS]),
				                        buf, sizeof(buf)) == NULL)
		return NULL;

	dev_name = (char*)RTA_DATA(tb[IFLA_IFNAME]);

	/*skip bridges*/
	if (namelist_search(dev_name, &bridge_names)) {
		return NULL;
	}

	if_info = netinfo_new();
	if (if_info == NULL)
		return NULL;

	strncpy(if_info->name, dev_name, NAME_LENGTH-1);

	strncpy(if_info->mac, buf, MAC_LENGTH);
	if_info->mac[MAC_LENGTH] = '\0';

	for ( ;ipinfo ; ipinfo = ipinfo->next) {
		struct nlmsghdr *n = &ipinfo->h;
		struct ifaddrmsg *ifa = NLMSG_DATA(n);

		if (n->nlmsg_type != RTM_NEWADDR)
			continue;

		if (n->nlmsg_len < NLMSG_LENGTH(sizeof(ifa)))
			break;

		if (ifa->ifa_index != (unsigned)ifi->ifi_index ||
		    (ifa->ifa_family != AF_INET))
			continue;

		if (put_addrinfo(n, if_info) < 0) {
			netinfo_free(if_info);
			return 0;
		}
	}

	for ( ;ip6info ; ip6info = ip6info->next) {
		struct nlmsghdr *n = &ip6info->h;
		struct ifaddrmsg *ifa = NLMSG_DATA(n);

		if (n->nlmsg_type != RTM_NEWADDR)
			continue;

		if (n->nlmsg_len < NLMSG_LENGTH(sizeof(ifa)))
			break;

		if (ifa->ifa_index != (unsigned)ifi->ifi_index ||
		    (ifa->ifa_family != AF_INET6))
			continue;

		if (put_addrinfo(n, if_info) < 0) {
			netinfo_free(if_info);
			return 0;
		}
	}

	return if_info;
}



static int read_ifconfioctl(struct netinfo **netinfo_head)
{
	struct rtnl_handle rth;
	struct nlmsg_list *linfo = NULL; //ETHs
	struct nlmsg_list *ipinfo = NULL; //IPs
	struct nlmsg_list *ip6info = NULL; //IPv6
	struct nlmsg_list *rinfo = NULL; //route
	struct nlmsg_list *r6info = NULL; //IPv6 route
	struct nlmsg_list *l, *n;
	int ret;

	if (rtnl_open(&rth, 0) < 0)
		return 1;

	ret = rtnl_wilddump_request(&rth, AF_UNSPEC, RTM_GETLINK);
	if (ret < 0) {
		werror("Cannot send dump request");
		goto out;
	}

	ret = rtnl_dump_filter(&rth, store_nlmsg, &linfo);
	if (ret < 0) {
		werror("Dump terminated");
		goto out;
	}

	//get IPv4
	ret = rtnl_wilddump_request(&rth, AF_INET, RTM_GETADDR);
	if (ret < 0) {
		werror("Cannot send dump request");
		goto out2;
	}

	ret = rtnl_dump_filter(&rth, store_nlmsg, &ipinfo);
	if (ret < 0) {
		werror("Dump terminated");
		goto out2;
	}

	ret = rtnl_wilddump_request(&rth, AF_INET, RTM_GETROUTE);
	if (ret < 0) {
		werror("Cannot send dump request");
		goto out2;
	}

	ret = rtnl_dump_filter(&rth, store_nlmsg, &rinfo);
	if (ret < 0) {
		werror("Dump terminated");
		goto out2;
	}

	//get IPv6
	ret = rtnl_wilddump_request(&rth, AF_INET6, RTM_GETADDR);
	if (ret < 0) {
		werror("Cannot send dump request");
		goto out2;
	}

	ret = rtnl_dump_filter(&rth, store_nlmsg, &ip6info);
	if (ret < 0) {
		werror("Dump terminated");
		goto out2;
	}

	//get IPv6 route
	ret = rtnl_wilddump_request(&rth, AF_INET6, RTM_GETROUTE);
	if (ret < 0) {
		werror("Cannot send dump request");
		goto out2;
	}

	ret = rtnl_dump_filter(&rth, store_nlmsg, &r6info);
	if (ret < 0) {
		werror("Dump terminated");
		goto out2;
	}

	for (l = linfo; l; l = n) {
		n = l->next;
		struct netinfo *if_info = put_linkinfo(&l->h, ipinfo, ip6info);
		if (if_info) {
			netinfo_add(if_info, netinfo_head);
		}
		free(l);
	}

	clean_nlmsg( ipinfo );
	clean_nlmsg( ip6info );

	for ( l = rinfo  ; l ;  l = n) {
		n = l->next;
		put_gateway(&l->h, netinfo_head, 0);
		free(l);
	}

	for ( l = r6info  ; l ;  l = n) {
		n = l->next;
		put_gateway(&l->h, netinfo_head, 1);
		free(l);
	}

out:
	rtnl_close(&rth);
	return 0;
out2:
	clean_nlmsg( linfo );
	clean_nlmsg( ipinfo );
	clean_nlmsg( rinfo );

	clean_nlmsg( ip6info );
	clean_nlmsg( r6info );

	goto out;
}



void read_dns(struct netinfo **netinfo_head){
	int i;

	if( res_init() != 0 )
		return;

	res_state resState = __res_state();
	for( i = 0; i<resState->nscount; ++i ) {
		char buf[100];
		if( resState->nsaddr_list[i].sin_family != AF_INET
			&& resState->nsaddr_list[i].sin_family != AF_INET6 )
			continue;

		unsigned int addr = resState->nsaddr_list[i].sin_addr.s_addr;
		inet_ntop( resState->nsaddr_list[i].sin_family,
						&addr, buf, sizeof(buf));

		struct netinfo *it = netinfo_get_first(netinfo_head);
		while (it != NULL){
			namelist_add(buf, &it->dns);
			it= it->next;
		}
	}

	for( i = 0; i< MAXNS; i++ ) {
		char buf[100];
		struct sockaddr_in6 *addr = resState->_u._ext.nsaddrs[i];
		if (addr == NULL)
			continue;
		if( addr->sin6_family != AF_INET
			&& addr->sin6_family != AF_INET6 )
			continue;

		inet_ntop( addr->sin6_family, &(addr->sin6_addr), buf, sizeof(buf));

		struct netinfo *it = netinfo_get_first(netinfo_head);
		while (it != NULL){
			namelist_add(buf, &it->dns);
			it= it->next;
		}
	}



	for ( i = 0; i < MAXDNSRCH; i++)
	{
		if (!resState->dnsrch[i])
			continue;
		struct netinfo *it = netinfo_get_first(netinfo_head);
		while(it) {
			namelist_add(resState->dnsrch[i], &it->search);
			it = it->next;
		}
	}
}


void read_dhcp(struct netinfo **netinfo_head)
{
	struct netinfo *info;

	info = netinfo_get_first(netinfo_head);
	while(info && strlen(info->mac))
	{
		char cmd[PATH_MAX+1];
		int rc;

		if (os_script_prefix == NULL)
			return;

		if (snprintf(cmd, PATH_MAX,
				SCRIPT_DIR "/%s-get_dhcp.sh \"%s\" \"%s\" 4", os_script_prefix, info->mac, info->name) >= PATH_MAX)
		{
			werror("ERROR: Command line for execution %s-get_dhcp.sh is too long", os_script_prefix);
			return;
		}

		rc = run_cmd(cmd);

		if (rc == 0)
			info->configured_with_dhcp = 1;
		else if(rc == 1)
			info->configured_with_dhcp = 0;
		else
			werror("Failed to get DHCP configuration for mac '%s'. return %d", info->mac, rc);


		if (snprintf(cmd, PATH_MAX,
				SCRIPT_DIR "/%s-get_dhcp.sh \"%s\" \"%s\" 6", os_script_prefix, info->mac, info->name) >= PATH_MAX)
		{
			werror("ERROR: Command line for execution %s-get_dhcp.sh is too long", os_script_prefix);
			return;
		}

		rc = run_cmd(cmd);

		if (rc == 0)
			info->configured_with_dhcpv6 = 1;
		else if(rc == 1)
			info->configured_with_dhcpv6 = 0;
		else
			werror("Failed to get DHCP configuration for mac '%s'. return %d", info->mac, rc);


		info = info->next;
	}

}

void detect_distribution()
{
	get_distribution(&os_vendor, &os_script_prefix);
}

int get_device_list(struct netinfo **netinfo_head) {

	get_distribution(&os_vendor, &os_script_prefix);

	read_bridge_info();
	read_ifconfioctl(netinfo_head);
	read_dns(netinfo_head);
	read_dhcp(netinfo_head);

	return 0;
}



