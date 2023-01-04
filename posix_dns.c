#include "posix_dns.h"
#include "namelist.h"
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <resolv.h>


#ifdef __FreeBSD__
#ifndef res_private_h
struct __res_state_ext {
        union res_sockaddr_union nsaddrs[MAXNS];
        struct sort_list {
                int     af;
                union {
                        struct in_addr  ina;
                        struct in6_addr in6a;
                } addr, mask;
        } sort_list[MAXRESOLVSORT];
        char nsuffix[64];
        char nsuffix2[64];
        struct timespec conf_mtim;      /* mod time of loaded resolv.conf */
        time_t          conf_stat;      /* time of last stat(resolv.conf) */
        u_short reload_period;          /* seconds between stat(resolv.conf) */
};
#endif
#endif

static void add_to_dns_namelist(struct netinfo **netinfo_head, const char *addr, bool search)
{
	struct netinfo *it;

	it = netinfo_get_first(netinfo_head);
	while (it != NULL) {
		namelist_add(addr, (search) ? &it->search : &it->dns);
		it = it->next;
	}
}

void read_dns(struct netinfo **netinfo_head)
{
	struct sockaddr_in6 *saddr;
	unsigned int addr;
	char buf[INET6_ADDRSTRLEN];
	int i;

	if( res_init() != 0 )
		return;

	res_state resState = __res_state();
	for (i = 0; i<resState->nscount; ++i) {
		if (resState->nsaddr_list[i].sin_family != AF_INET
			&& resState->nsaddr_list[i].sin_family != AF_INET6 )
			continue;

		addr = resState->nsaddr_list[i].sin_addr.s_addr;
		inet_ntop(resState->nsaddr_list[i].sin_family, &addr, buf, sizeof(buf));
		add_to_dns_namelist(netinfo_head, buf, false);
	}

	for (i = 0; i< MAXNS; i++) {
#if defined(__FreeBSD__)
		saddr = &resState->_u._ext.ext->nsaddrs[i].sin6;
		if (memcmp(&(saddr->sin6_addr), &in6addr_any, sizeof(in6addr_any)) == 0)
			continue;
#elif defined(__linux__)
		saddr = resState->_u._ext.nsaddrs[i];
#else
#error Unsupported platform
#endif
		if (saddr == NULL)
			continue;

		if (saddr->sin6_family != AF_INET
			&& saddr->sin6_family != AF_INET6)
			continue;

		inet_ntop(saddr->sin6_family, &(saddr->sin6_addr), buf, sizeof(buf));
		add_to_dns_namelist(netinfo_head, buf, false);
	}

	for (i = 0; i < MAXDNSRCH; i++) {
		if (!resState->dnsrch[i])
			continue;
		add_to_dns_namelist(netinfo_head, resState->dnsrch[i], true);
	}
}
