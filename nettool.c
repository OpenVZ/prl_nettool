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
 * main() and prints output
 */

/* #define DEBUG_OPT_COMPARE 1 */

#ifdef _WIN_
#include <sdkddkver.h>
#include <windows.h>
#include <cfgmgr32.h>
#else
#define _GNU_SOURCE
#include <dirent.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "common.h"
#include "options.h"
#include "netinfo.h"
#include "setnet.h"
#include "namelist.h"

/* 2 mins to wait for PnP and SCM start completed */
#define SCM_TIMEOUT (120*1000)

extern struct nettool_options net_opts;

int print_parameters()
{
	unsigned int all_flags[] = {NET_OPT_GATEWAY, NET_OPT_DNS,  NET_OPT_IP,
					NET_OPT_DHCP, NET_OPT_SEARCH, 0};
	struct netinfo *netinfo_head;
	int i;


	if (!is_opt_set(NET_OPT_GETBYMAC) && !is_opt_set(NET_OPT_GETNOTMAC)
		&& count_opt_mac(NET_OPT_GETBYMAC) == 0)
		return 0;//nothing to do

	netinfo_head = NULL;

	//get ALL information in system
	get_device_list(&netinfo_head);

	for (i = 0; all_flags[i]; i++)
	{
		struct netinfo *if_it = NULL;
		unsigned int opt = all_flags[i];
		int show_by_mac = 1, count_macs = count_opt_mac(opt);

		if (!is_opt_set( opt ) && count_macs == 0)
			continue;

		if (is_opt_set( opt ))
			show_by_mac = 0;

		for (if_it = netinfo_head; if_it != NULL; if_it = if_it->next) //all scanned devices
		{

			if (show_by_mac)
			{
				if (search_opt_mac(if_it->mac, opt) == 0)
				{//not found
					continue;
				}
			}

			if (opt == NET_OPT_GATEWAY && if_it->gateway)
			{
				printf("GATEWAY;%s;", if_it->mac);
				print_namelist(&if_it->gateway);
				printf("\n");
			}
			else if (opt == NET_OPT_DNS && if_it->dns)
			{
				printf("DNS;%s;", if_it->mac);
				print_namelist(&if_it->dns);
				printf("\n");
			}
			else if (opt == NET_OPT_IP && if_it->ip)
			{
				printf("IP;%s;", if_it->mac);
				print_namelist(&if_it->ip);
				printf("\n");

			}
			else if (opt == NET_OPT_DHCP)
			{
				printf("DHCP;%s;", if_it->mac);
				if (if_it->configured_with_dhcp)
					printf("TRUE");
				else
					printf("FALSE");
				printf("\n");

				printf("DHCPV6;%s;", if_it->mac);
				if (if_it->configured_with_dhcpv6)
					printf("TRUE");
				else
					printf("FALSE");
				printf("\n");
			}

		} //while over network interfaces

		if (netinfo_head != NULL && opt == NET_OPT_SEARCH && netinfo_head->search)
		{
			printf("SEARCHDOMAIN;");
			print_namelist(&netinfo_head->search);
			printf("\n");

		}

	}

	return 0;
}

int is_equal_dhcp(struct netinfo *if_it, struct nettool_mac *mac_it)
{
	int dhcpv4 = (strchr(mac_it->value, '4') != NULL);
	int dhcpv6 = (strchr(mac_it->value, '6') != NULL);

#ifdef DEBUG_OPT_COMPARE
	debug("DHCP;%s;%s\n", if_it->mac, mac_it->value);
	debug("configured_with_dhcp=%d configured_with_dhcpv6=%d\n",
	      if_it->configured_with_dhcp, if_it->configured_with_dhcpv6);
	fflush(stdout);
#endif

	if (!net_opts.compare)
		return 0;

	return (dhcpv4 == if_it->configured_with_dhcp &&
		dhcpv6 == if_it->configured_with_dhcpv6);
}

static int is_equal_ip_skip_local(struct netinfo *if_it, const char *str, const char *delim)
{
	struct namelist *it = NULL;
	char * tmp, *s;

	if (if_it->ip == NULL || str == NULL)
		return (if_it->ip == NULL && str == NULL);

	tmp = strdup(str);
	if (tmp == NULL) {
		error(0, "ERROR: failed to strdup");
		return 0;
	}

	for (s = strtok(tmp, delim); s != NULL; s = strtok(NULL, delim)) {
		if (strcasestr(s, "remove") != NULL)
			continue;
		if (!namelist_search(s, &if_it->ip)) {
			free(tmp);
			return 0;
		}
	}

	free(tmp);

	for (it = if_it->ip; it != NULL; it = it->next) {
		if (!it->name)
			continue;
		if (namelist_search(it->name, &if_it->ip_link))
			//skip link-local and site-local
			continue;
		if (strcasestr(str, it->name) == NULL)
			return 0;
	}

	return 1;
}

int is_equal_ip(struct netinfo *if_it, struct nettool_mac *mac_it)
{
#ifdef DEBUG_OPT_COMPARE
	char str[1024];
	debug("IP_OPT;%s;%s\n", if_it->mac, mac_it->value);
	print_namelist_to_str(&if_it->ip, str, sizeof(str));
	debug("IP;%s", str);
	print_namelist_to_str(&if_it->ip_link, str, sizeof(str));
	debug("IP_LINK;%s", str);
#endif

	if (!net_opts.compare)
		return 0;

	return is_equal_ip_skip_local(if_it, mac_it->value, " ");
}

int is_equal_dns(struct netinfo *if_it, struct nettool_mac *mac_it)
{
#ifdef DEBUG_OPT_COMPARE
	char str[1024];
	debug("DNS_OPT;%s;%s\n", if_it->mac, mac_it->value);
	print_namelist_to_str(&if_it->dns, str, sizeof(str));
	debug("DNS;%s", str);
#endif
	if (!net_opts.compare)
		return 0;

	return namelist_compare(&if_it->dns, mac_it->value, " ");
}

int is_equal_gateway(struct netinfo *if_it, struct nettool_mac *mac_it)
{
#ifdef DEBUG_OPT_COMPARE
	char str[1024];
	debug("GW_OPT;%s;%s\n", if_it->mac, mac_it->value);
	print_namelist_to_str(&if_it->gateway, str, sizeof(str));
	debug("GW;%s", str);
#endif
	if (!net_opts.compare)
		return 0;

	return namelist_compare(&if_it->gateway, mac_it->value, " ");
}

#if defined(_WIN_)
static void do_disable_adapter(struct netinfo* adapter_)
{
	if (adapter_->disabled)
		return;

	enable_adapter(adapter_, 0);
	adapter_->disabled = 1;
}

static void disable_adapter_if_needed(struct netinfo *if_it)
{
#if (NTDDI_VERSION < NTDDI_LONGHORN)
	//windows 2k3 don't like if we set IP/mask/gateway
	//from differend ranges
	//workaround: disable adapter, update setting, enable adapter
	if (get_opt_mac(if_it->mac, NET_OPT_GATEWAY) == NULL &&
	    get_opt_mac(if_it->mac, NET_OPT_ROUTE) == NULL)
		return;

	//disable
	do_disable_adapter(if_it);
#endif // NTDDI_VERSION < NTDDI_LONGHORN
}

void enable_adapter_if_needed(struct netinfo *if_it)
{
	if (!if_it->disabled)
		return;

	// enable
	enable_adapter(if_it, 1);
	if_it->disabled = 0;
}
#endif // _WIN_

struct nettool_mac *get_opt_val_substr(struct nettool_mac *mac_it, unsigned int opts, const char *value)
{
	while(mac_it != NULL)
	{
		if ((mac_it->type & opts) && mac_it->value != NULL && strstr(value, mac_it->value) != NULL)
			return mac_it;

		mac_it = mac_it->next;
	}
	return NULL;
}

struct nettool_mac *get_opt_val_no_substr(struct nettool_mac *mac_it, unsigned int opts, const char *value)
{
	while(mac_it != NULL)
	{
		if ((mac_it->type & opts) && mac_it->value != NULL && strstr(value, mac_it->value) == NULL)
			return mac_it;

		mac_it = mac_it->next;
	}
	return NULL;
}

int apply_parameters()
{
	unsigned int all_flags[] = { NET_OPT_DHCP, NET_OPT_IP, NET_OPT_GATEWAY, NET_OPT_SEARCH,
					NET_OPT_DNS, NET_OPT_ROUTE, NET_OPT_HOSTNAME, 0};
	struct netinfo *netinfo_head;
	struct nettool_mac *mac_it;
	int rc = 0, rc2 = 0;
	int i;
	int enable_count = 0;
	struct netinfo *if_it = NULL;

	if (count_opt_mac(NET_OPT_GETBYMAC) == 0 && count_opt_mac(NET_OPT_GETNOTMAC) == 0)
		return 0;//nothing to do

	netinfo_head = NULL;

	//get ALL information in system
	get_device_list(&netinfo_head);


#if defined(_WIN_) && (NTDDI_VERSION >= NTDDI_LONGHORN)
	//try to enable #PSBM-9809
	//on win2k3 we can't detect MAC address of disabled adapter
	//enable only on w2k8 and above
	{
		struct namelist *wait_adapters = NULL;

		mac_it = net_opts.macs;
		while(mac_it != NULL)
		{
			if (mac_it->mac != NULL &&
				netinfo_search_mac(&netinfo_head, mac_it->mac) == NULL)
			{
				error(0, "Info: Enabling '%s' ...", mac_it->mac);
				if (enable_disabled_adapter(mac_it->mac) > 0)
					namelist_add(mac_it->mac, &wait_adapters);

			}
			mac_it = mac_it->next;
		}

		if (wait_adapters != NULL)
		{
			wait_for_start(wait_adapters);
			namelist_clean(&wait_adapters);
		}
	}
#else
	VARUNUSED(enable_count);
#endif

	//check MACs from options
	mac_it = net_opts.macs;
	while(mac_it != NULL)
	{
		if (mac_it->mac != NULL &&
			netinfo_search_mac(&netinfo_head, mac_it->mac) == NULL)
		{
			error(0, "WARNING: MAC address '%s' was not found in system", mac_it->mac);
			//exclude from options - just clean option type
			mac_it->type = 0;
		}
		mac_it = mac_it->next;
	}

#ifdef _MAC_
	OpenEdit();
#endif

	for (i = 0; all_flags[i]; i++)
	{
		struct netinfo *if_it = NULL;
		unsigned int opt = all_flags[i];

		if (count_opt_mac(opt) == 0)
			continue;

		for (if_it = netinfo_head; if_it != NULL; if_it = if_it->next) //over all scanned devices
		{
			struct nettool_mac *mac_it = get_opt_mac(if_it->mac, opt);
			if (mac_it == NULL)
			{//not found
				continue;
			}
#ifdef DEBUG_OPT_COMPARE
			if (opt == NET_OPT_DHCP)
				debug("is_equal_dhcp(if_it, mac_it)=%d\n", is_equal_dhcp(if_it, mac_it));
			if (opt == NET_OPT_IP)
				debug("is_equal_ip(if_it, mac_it)=%d\n", is_equal_ip(if_it, mac_it));
			if (opt == NET_OPT_GATEWAY)
				debug("is_equal_gw(if_it, mac_it)=%d\n", is_equal_gateway(if_it, mac_it));
			if (opt == NET_OPT_DNS)
				debug("is_equal_dns(if_it, mac_it)=%d\n", is_equal_dns(if_it, mac_it));
#endif
			if (!net_opts.compare ||
			    /* Always do reconfigure if dhcp specified.
			     * quick fix for bug #PSBM-16222.
			     * The is_equal_dhcp do not take into account IP change
			     * (opt == NET_OPT_DHCP && !is_equal_dhcp(if_it, mac_it)) || */
			    (opt == NET_OPT_DHCP) ||
			    (opt == NET_OPT_IP && !is_equal_ip(if_it, mac_it)) ||
			    (opt == NET_OPT_GATEWAY && !is_equal_gateway(if_it, mac_it)) ||
			    (opt == NET_OPT_DNS && !is_equal_dns(if_it, mac_it)))
				if_it->changed = 1;
		}
	}

	for (i = 0; all_flags[i]; i++)
	{
		unsigned int opt = all_flags[i];

		if (count_opt_mac(opt) == 0)
			continue;

		for (if_it = netinfo_head; if_it != NULL; if_it = if_it->next) //over all scanned devices
		{
			struct nettool_mac *mac_it = get_opt_mac(if_it->mac, opt);
			if (mac_it == NULL)
			{//not found
				continue;
			}

			if (!if_it->changed)
				continue;
#if defined(_WIN_)
			disable_adapter_if_needed(if_it);
#endif // _WIN_
			if (opt == NET_OPT_DHCP)
				rc = set_dhcp(if_it, mac_it);
			else if (opt == NET_OPT_ROUTE)
				rc = set_route(if_it, mac_it);
			else if (opt == NET_OPT_DNS)
				rc = set_dns(if_it, mac_it);
			else if (opt == NET_OPT_IP)
				rc = set_ip(if_it, mac_it);

			if (rc)
				rc2 = rc;

		} //over network interfaces

		if (opt == NET_OPT_GATEWAY)
		{
			struct nettool_mac *mac_it;

			// remove gateway first
			mac_it = get_opt_val_substr(net_opts.macs, NET_OPT_GATEWAY, "remove");
			while (mac_it != NULL) {
				if_it = netinfo_search_mac(&netinfo_head, mac_it->mac);
				if (if_it) {
					rc = set_gateway(if_it, mac_it);
					if (rc)
						rc2 = rc;
				}
				mac_it = get_opt_val_substr(mac_it->next, NET_OPT_GATEWAY, "remove");
			}

			// then set
			mac_it = get_opt_val_no_substr(net_opts.macs, NET_OPT_GATEWAY, "remove");
			while (mac_it != NULL) {
				if_it = netinfo_search_mac(&netinfo_head, mac_it->mac);
				if (if_it) {
					rc = set_gateway(if_it, mac_it);
					if (rc)
						rc2 = rc;
				}
				mac_it = get_opt_val_no_substr(mac_it->next, NET_OPT_GATEWAY, "remove");
			}
		}

		if (opt == NET_OPT_SEARCH)
		{
			struct nettool_mac *mac_it = get_opt_mac(NULL, opt);
			int rc;
			if (mac_it == NULL)
				continue;
			//set search domains
			rc = set_search_domain(mac_it);
			if (rc)
				rc2 = rc;
		}

		if (opt == NET_OPT_HOSTNAME)
		{
			struct nettool_mac *mac_it = get_opt_mac(NULL, opt);
			int rc;
			if (mac_it == NULL)
				continue;
			rc = set_hostname(mac_it);
			if (rc)
				rc2 = rc;
		}
	}

#ifdef _MAC_
	SavePrefs();
#endif

#if defined(_WIN_)
	//over all scanned devices
	for (if_it = netinfo_head; if_it != NULL; if_it = if_it->next)
	{
#if (NTDDI_VERSION >= NTDDI_LONGHORN)
		/*
		 * the adapter restart is required to apply dhcp
		 * settings on longhorn windows and newer. see
		 * #PSBM-18905.
		 * */
		if (NULL != get_opt_mac(if_it->mac, NET_OPT_DHCP))
			do_disable_adapter(if_it);
#endif // NTDDI_VERSION >= NTDDI_LONGHORN
		enable_adapter_if_needed(if_it);
	}
#endif // _WIN_
	return rc2;
}

int clean_parameters()
{

	struct netinfo *netinfo_head, *if_it;

	netinfo_head = NULL;

	//get ALL information in system
	get_device_list(&netinfo_head);


#ifdef _MAC_
	OpenEdit();
#endif


		for (if_it = netinfo_head; if_it != NULL; if_it = if_it->next) //over all scanned devices
		{
			int rc = 0;


			if (!if_it->configured_with_dhcp)
				continue;


			rc = clean(if_it);


			if (rc)
			{
				goto exit;
			}

		} //over network interfaces


exit:
#ifdef _MAC_
	SavePrefs();
#endif
	return 0;
}

int restart_network()
{
#ifdef _LIN_
	detect_distribution();
#endif

	return restart_guest_network();
}


#define MUTEX_TIMEOUT	240
#ifdef _WIN_
#define MUTEX_NAME	"Global\\prl_nettool_mutex"
static HANDLE hMutex;
#else
#define MUTEX_NAME "/tmp/prl_nettool.lock"
static int fdlock = -1;
#endif

void single_app_lock(void)
{
#ifdef _WIN_
	hMutex = CreateMutexA(NULL, FALSE, MUTEX_NAME);
	if (hMutex == NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
		hMutex = OpenMutexA(SYNCHRONIZE, FALSE, MUTEX_NAME);
	}
	if (hMutex == NULL) {
		fprintf(stderr, "WARNING: OpenMutex(\"%s\") = %u\n", MUTEX_NAME, (int)GetLastError());
	} else {
		switch (WaitForSingleObject(hMutex, MUTEX_TIMEOUT*1000))
		{
		case WAIT_FAILED:
		case WAIT_ABANDONED:
			fprintf(stderr, "ERROR: WaitForSingleObject(\"%s\") = %u\n", MUTEX_NAME, (int)GetLastError());
			exit(2);
			break;
		case WAIT_TIMEOUT:
			fprintf(stderr, "ERROR: timeout waiting for pending operation to finish\n");
			exit(2);
			break;
		}
	}
#else
	struct flock fl;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

	if ((fdlock = open(MUTEX_NAME, O_WRONLY|O_CREAT, 0600)) == -1) {
		fprintf(stderr, "WARNING: open(\"%s\") = %d\n", MUTEX_NAME, errno);
	} else {
		int count = MUTEX_TIMEOUT;
		while (fcntl(fdlock, F_SETLK, &fl) == -1) {
			if (errno == EAGAIN || errno == EACCES) {
				if (--count == 0) {
					fprintf(stderr, "ERROR: timeout waiting for pending operation to finish\n");
					exit(2);
				}
				sleep(1);
			} else {
				fprintf(stderr, "WARNING: fcntl(\"%s\", F_SETLK) = %d\n", MUTEX_NAME, errno);
				break;
			}
		}
	}
#endif
}

void single_app_unlock(void)
{
#ifdef _WIN_
	if (hMutex)
		ReleaseMutex(hMutex);
#endif
}

int do_work()
{
	int rc = 1;

	debug("%s enter", __FUNCTION__);

	single_app_lock();

	debug("%s single_app_lock() done", __FUNCTION__);

#ifdef _WIN_
	HANDLE h = OpenEventW(SYNCHRONIZE, FALSE, L"SC_AutoStartComplete");
	DWORD r = ERROR_INVALID_HANDLE;
	if (h != NULL) {
		r = WaitForSingleObject(h, SCM_TIMEOUT);
		CloseHandle(h);
	}
	debug("wait for SCM completed: %d", (int)r);

	r = CMP_WaitNoPendingInstallEvents(SCM_TIMEOUT);
	debug("wait for PNP completed: %d", (int)r);

#if (NTDDI_VERSION < NTDDI_LONGHORN)
	//on win2k3 prl_nettool disable/enable adapter in apply_parameters().
	//first - restore adapters #PSBM-9930
	restore_adapter_state();
#endif
	//#PSBM-9930 and #PSBM-35109
	//sometimes initial pnp configuration of network adapters takes much time
	//we need to wait at least for GetAdaptersInfo() to be successfull
	//and for all requested adapters to be in returned list
	{
		struct namelist *wait_adapters = NULL;
		struct nettool_mac *mac_it;

		// wait for adapters to appear
		mac_it = net_opts.macs;
		while(mac_it != NULL)
		{
			if (mac_it->mac != NULL)
				namelist_add(mac_it->mac, &wait_adapters);
			mac_it = mac_it->next;
		}
		if (wait_adapters != NULL)
		{
			wait_for_start(wait_adapters);
			namelist_clean(&wait_adapters);
		}
	}
#endif

	if (net_opts.action == GET)
		rc = print_parameters();
	else if (net_opts.action == SET)
		rc = apply_parameters();
	else if  (net_opts.action == CLEAN)
		rc = clean_parameters();
	else if  (net_opts.action == RESTART)
		rc = restart_network();
	else
		error(0, "ERROR: unknown action %d", net_opts.action);

	single_app_unlock();

	debug("%s return %d", __FUNCTION__, rc);
	return rc;
}

#ifdef _WIN_
int __cdecl  main(int argc, char* argv[])
#else
int main(int argc, char* argv[])
#endif
{
	VARUNUSED(argc);

	debug("%s started", argv[0]);

#if defined(_WIN_) && (NTDDI_VERSION < NTDDI_LONGHORN)
	//if current vista or above, then run prl_nettool_vista
	if (is_ipv6_supported())
	{
		char cmd[4000];
		DWORD rc = 0;
		PROCESS_INFORMATION procinfo = { 0 };
		STARTUPINFOW startinfo = { 0 };
		WCHAR cmdline[4000];

		debug("is_ipv6_supported true");

		sprintf(cmd, "%s", "prl_nettool_vista.exe");
		argv++;
		while (*argv != NULL)
		{
			if (strchr(*argv, ' ') != NULL) {
				strcat(cmd, " \"");
				strcat(cmd, *argv);
				strcat(cmd, "\"");
			} else {
				strcat(cmd, " ");
				strcat(cmd, *argv);
			}

			argv++;
		}

		debug("exec %s", cmd);

		startinfo.cb = sizeof(startinfo);
		_snwprintf(cmdline, sizeof(cmdline)/sizeof(*cmdline), L"%hs", cmd);
		if (!CreateProcessW(NULL,
					cmdline,		// command line
					NULL,		// process security attributes
					NULL,		// primary thread security attributes
					FALSE,		// handles are inherited
					0,			// creation flags
					NULL,		// use parent's environment
					NULL,		// use parent's current directory
					&startinfo,		// STARTUPINFO pointer
					&procinfo))		// receives PROCESS_INFORMATION
		{
			rc = GetLastError();
			error(rc, "Failed to exec %s", cmd);
			exit(rc);
		}

		WaitForSingleObject(procinfo.hProcess, INFINITE);
		GetExitCodeProcess(procinfo.hProcess, &rc);
		CloseHandle(procinfo.hProcess);
		CloseHandle(procinfo.hThread);

		return rc;
	}
#endif // _WIN_ && NTDDI_VERSION < NTDDI_LONGHORN)

#ifdef _LIN_
	//save command to syslog
	char cmd[PATH_MAX], tmp[PATH_MAX];
	sprintf(cmd, "%s", "prl_nettool");
	char **ar = argv;
	while (*ar != NULL)
	{
		strcpy(tmp, cmd);

		if (strchr(*ar, ' ') != NULL)
			snprintf(cmd, PATH_MAX, "%s \"%s\"", tmp, *ar);
		else
			snprintf(cmd, PATH_MAX, "%s %s", tmp, *ar);

		ar++;
	}

	openlog("prl_nettool", LOG_PID | LOG_CONS, LOG_USER);
	syslog(LOG_ERR | LOG_USER, "call: %s", cmd);
	closelog();
#endif

	parse_options(argv);
	do_work();
	return 0;
}




