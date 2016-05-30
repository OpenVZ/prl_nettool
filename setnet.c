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
 * methods for setting network parameters
 */

#include <sdkddkver.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <io.h>
#include <fcntl.h>
#include <ctype.h>

#include "common.h"
#include "netinfo.h"
#include "namelist.h"
#include "options.h"

#define TOOL_GUEST_UTILS_REGISTRY_PATH "Software\\Virtuozzo\\prl_nettool"


typedef LONG    DNS_STATUS;

typedef enum _DNS_NAME_FORMAT {
    DnsNameDomain,
    DnsNameDomainLabel,
    DnsNameHostnameFull,
    DnsNameHostnameLabel,
    DnsNameWildcard,
    DnsNameSrvRecord,
    DnsNameValidateTld
} DNS_NAME_FORMAT;

DNS_STATUS WINAPI DnsValidateName_A(PCSTR pszName, DNS_NAME_FORMAT Format);

static int exec_cmd(char *cmd)
{
	DWORD rc = 0;
	PROCESS_INFORMATION procinfo = { 0 };
	STARTUPINFOW startinfo = { 0 };
	WCHAR cmdline[1024];

	debug("run: %s\n", cmd);
	startinfo.cb = sizeof(startinfo);
	_snwprintf(cmdline, sizeof(cmdline)/sizeof(*cmdline), L"%hs", cmd);
	cmdline[sizeof(cmdline)/sizeof(*cmdline)-1] = 0;
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
		return rc;
	}

	WaitForSingleObject(procinfo.hProcess, INFINITE);
	GetExitCodeProcess(procinfo.hProcess, &rc);
	CloseHandle(procinfo.hProcess);
	CloseHandle(procinfo.hThread);

	if (rc)
		error(0, "failed to run cmd: %s. return %d (0x%X)", cmd, rc, rc);

	return rc;
}

static int exec_netsh(char *text)
{
	char ex_buf[1024];
	sprintf(ex_buf, "netsh %s", text);
	return exec_cmd(ex_buf);
}

int remove_ipv6_ips(struct netinfo *if_it)
{
	char str[1024];
	int rc;
	struct namelist *ip_mask = if_it->ip;
	char ip[100], mask[IP_LENGTH+1];

	while (ip_mask != NULL)
	{
		if (!is_ipv6(ip_mask->name) ||
			namelist_search(ip_mask->name, &if_it->ip_link))
		{
			//skip link-local and site-local
			ip_mask = ip_mask->next;
			continue;
		}

		if (split_ip_mask(ip_mask->name, ip, mask) < 0)
					return -1;
		sprintf(str, "interface ipv6 del address \"%s\" %s", if_it->name, ip);
		if ((rc = exec_netsh(str)))
					return -1;

		ip_mask = ip_mask->next;
	}

	return 0;
}

// return 1st not link-local addr of iface
static int find_if_first_ip(struct netinfo *if_it, char * ip)
{
	struct namelist *ip_mask;
	char mask[IP_LENGTH+1];

	for (ip_mask = if_it->ip; ip_mask != NULL; ip_mask = ip_mask->next)
	{
		if (namelist_search(ip_mask->name, &if_it->ip_link))
			//skip link-local and site-local
			continue;

		if (split_ip_mask(ip_mask->name, ip, mask) < 0)
			return -1;

		return 0;
	}

	return -1;
}

int set_ip(struct netinfo *if_it, struct nettool_mac *params)
{
	char str[1024];
	int rc;
	struct namelist *ip_masks = NULL;
	struct namelist *ip_mask;
	char ip[100], mask[IP_LENGTH+1];
	int first_ipv4=1, first_ipv6=1, remove=0;
	struct nettool_mac *mac_it;

	//parse value
	namelist_split(&ip_masks, params->value);

	//for first
	ip_mask = ip_masks;
	while(ip_mask)
	{
		if (!strcmp(ip_mask->name, "remove"))
		{
			remove=1;
		}
		else if (!is_ipv6(ip_mask->name))
		{
			if (split_ip_mask(ip_mask->name, ip, mask) < 0)
					return -1;

#if (NTDDI_VERSION < NTDDI_LONGHORN)
			//workaround on win2k3
			if (!strcmp(mask, "255.255.255.255"))
				strcpy(mask, "255.255.255.254");
#endif

			if (first_ipv4)
			{

				sprintf(str, "interface ip set address \"%s\" static %s %s", if_it->name, ip, mask);
				if ((rc = exec_netsh(str)))
					return -1;
				if_it->configured_with_dhcp = 0;
				first_ipv4 = 0;
			}
			else
			{
				sprintf(str, "interface ip add address \"%s\" %s %s", if_it->name, ip, mask);
				rc = exec_netsh(str);
#if (NTDDI_VERSION >= NTDDI_LONGHORN)
				//on win2k3 during set second ip on disabled adapter - netsh return error without any
				//description. workaround: don't check exit code on w2k3. check on vista and later
				if (rc)
					return -1;
#endif
			}
		}
		else
		{
			if (first_ipv6)
			{
				//remove all IPs except of local-link
				if (remove_ipv6_ips(if_it))
					return -1;

				if (split_ip_mask(ip_mask->name, ip, mask) < 0)
					return -1;
				sprintf(str, "interface ipv6 set address \"%s\" %s/%s", if_it->name, ip, mask);
				if ((rc = exec_netsh(str)))
					return -1;
				if_it->configured_with_dhcpv6 = 0;
				first_ipv6 = 0;
			}
			else
			{
				if (split_ip_mask(ip_mask->name, ip, mask) < 0)
					return -1;
				sprintf(str, "interface ipv6 add address \"%s\" %s/%s", if_it->name, ip, mask);
				if ((rc = exec_netsh(str)))
					return -1;
			}
		}

		ip_mask = ip_mask->next;
	}

	if (is_ipv6_supported()
			&& first_ipv6 /*ipv6 IPs are not specified*/)
	{
		//remove all ipv6 configuration
		sprintf(str, "interface ipv6 set dns \"%s\" dhcp", if_it->name);
		exec_netsh(str);

		//remove all IPs except of local-link
		remove_ipv6_ips(if_it);
	}

	mac_it = get_opt_mac(if_it->mac, NET_OPT_DHCP);
	if (remove ||
		(first_ipv4 &&
		  (mac_it == NULL || mac_it->value == NULL || strchr(mac_it->value, '4') == NULL)) )
	{
		//remove all ipv4 configuration
		sprintf(str, "interface ip set address \"%s\" dhcp", if_it->name);
		exec_netsh(str);
	}

	namelist_clean(&ip_masks);

	return 0;
}


int set_dns(struct netinfo *if_it, struct nettool_mac *params)
{
	char str[1024];
	int rc;
	struct namelist *ips = NULL;
	struct namelist *ip;
	int clear_ipv4=0, clear_ipv6=0;


	if (params->value == NULL)
		return 0;

	if (strlen(params->value) == 0)
		return 0;

	//split params->value
	namelist_split(&ips, params->value);

	//for each ip do
	ip = ips;
	while (ip)
	{
		//set none
		if (!is_ipv6(ip->name))
		{
			if (!clear_ipv4)
			{
				sprintf(str, "interface ip set dns \"%s\" static none", if_it->name);
				clear_ipv4 = 1;
				if ((rc = exec_netsh(str)) < 0)
					return rc;
			}
		}
		else
		{
			if (!clear_ipv6)
			{
				sprintf(str, "interface ipv6 set dns \"%s\" static none", if_it->name);
				clear_ipv6 = 1;
				if ((rc = exec_netsh(str)) < 0)
					return rc;
			}
		}

		if (!is_ipv6(ip->name))
			sprintf(str, "interface ip add dns name=\"%s\" addr=%s",
										if_it->name, ip->name);
		else
			sprintf(str, "interface ipv6 add dns name=\"%s\" addr=%s",
										if_it->name, ip->name);
		if ((rc = exec_netsh(str))< 0)
			return rc;

		ip = ip->next;
	}
	namelist_clean(&ips);

	return 0;
}

int set_gateway(struct netinfo *if_it, struct nettool_mac *params)
{
	char str[1024];
	struct namelist *gws = NULL;
	struct namelist *gw;

	//if (if_it->configured_with_dhcp){
	//	error(0, "WARNING: configured with dhcp. don't change");
	//	return 0;
	//}

	//split params->value
	namelist_split(&gws, params->value);

	//for each ip do
	gw = gws;
	while (gw)
	{
		if (is_ipv6(gw->name) ||
				!strncmp(gw->name, NET_STR_OPT_REMOVEV6, strlen(NET_STR_OPT_REMOVEV6)))
			sprintf(str, "interface ipv6 del route ::/0 \"%s\"", if_it->name);
		else
#if (NTDDI_VERSION < NTDDI_LONGHORN)
			sprintf(str, "interface ip del address \"%s\" gateway=all", if_it->name);
#else
			sprintf(str, "interface ip del route 0.0.0.0/0 \"%s\"", if_it->name);
#endif
		exec_netsh(str);

		if (strlen(gw->name) > 0 &&
			strncmp(gw->name, NET_STR_OPT_REMOVE, strlen(NET_STR_OPT_REMOVE)) &&
			strncmp(gw->name, NET_STR_OPT_REMOVEV6, strlen(NET_STR_OPT_REMOVEV6))) {
			if (is_ipv6(gw->name))
				sprintf(str, "interface ipv6 add route ::/0 \"%s\" %s store=persistent",
							if_it->name, gw->name);
			else
				sprintf(str, "interface ip add address \"%s\" gateway=%s gwmetric=1",
							if_it->name, gw->name);
			exec_netsh(str);
		}

		gw = gw->next;
	}

	return 0;
}


int set_search_domain(struct netinfo *netinfo_header, struct nettool_mac *params)
{
	char domains[1024]; //to store value

	DWORD rc;
	HKEY hKey;
	ULONG len;
	struct namelist *names = NULL;
	struct namelist *name;

	if (params->value == NULL)
		return 0;

	domains[0] = '\0';
	if (strlen(params->value) > 0) {
		namelist_split(&names, params->value);
		name = names;
		while(name){
			int dlen = strlen(domains);
			if (dlen == 0)
				sprintf(domains + dlen, "%s", name->name);
			else
				sprintf(domains + dlen, ",%s", name->name);
			name = name->next;
		}
		namelist_clean(&names);
	}

	len = strlen(domains) + 1;

	rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, S_TCPPARAMS_REG_VALUE_KEY, 0, KEY_WRITE, &hKey);
	if (rc != ERROR_SUCCESS){
		error(rc, "Can't open registry %s\n", S_TCPPARAMS_REG_VALUE_KEY);
		return 1;
	}

	rc = RegSetValueExA(hKey, S_SEARCHLIST_REG_NAME, 0, REG_SZ, (LPBYTE)domains, len);
	if (rc != ERROR_SUCCESS){
		error(rc, "Can't set registry %s\\%s. return %d\n",
				S_TCPPARAMS_REG_VALUE_KEY, S_SEARCHLIST_REG_NAME, rc);
		RegCloseKey(hKey);
		return 1;
	}

	rc = RegCloseKey(hKey);

	return 0;
}

static int set_reg_value(const char *path, const char *key, const char *value)
{
	DWORD rc;
	HKEY hKey;
	ULONG len;

	rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_WRITE|KEY_QUERY_VALUE, &hKey);
	if (rc != ERROR_SUCCESS) {
		DWORD dwDisposition;
		rc = RegCreateKeyExA(HKEY_LOCAL_MACHINE, path, 0, NULL, REG_OPTION_NON_VOLATILE,
									KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
		if (rc != ERROR_SUCCESS) {
			error(rc, "Can't open/create registry %s\n", path);
			return 1;
		}
	}

	len = strlen(value) + 1;
	rc = RegSetValueExA(hKey, key, 0, REG_SZ, (LPBYTE)value, len);
	if (rc != ERROR_SUCCESS) {
		error(rc, "Can't set registry %s\\%s. return %d\n", path, key, rc);
		RegCloseKey(hKey);
		return 1;
	}

	rc = RegFlushKey(hKey);
	if (rc != ERROR_SUCCESS) {
		error(rc, "Can't flush registry %s. return %d\n", path, rc);
	}

	RegCloseKey(hKey);
	return 0;
}

static int get_reg_value(const char *path, const char *key, char **value)
{
	DWORD rc;
	HKEY hKey;
	ULONG len = 0;
	DWORD type = 0;
	char *buf;

	rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey);
	if (rc != ERROR_SUCCESS){
		error(rc, "Can't open registry for read %s", path);
		return 1;
	}

	rc = RegQueryValueExA(hKey, key, 0, &type, (LPBYTE)NULL, &len);
	if (rc != ERROR_MORE_DATA && rc != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		if (rc == ERROR_FILE_NOT_FOUND)
				return -1;
		error(rc, "Can't get registry value %s\\%s. return %d", path, key, rc);
		return 1;
	}

	if (type != REG_SZ) {
		error(0, "Wrong value type");
		return 1;
	}

	buf = malloc(len+1);
	if (buf == NULL) {
		error(rc, "Can't malloc(%d). return %d", len+1, rc);
		RegCloseKey(hKey);
		return 1;
	}

	rc = RegQueryValueExA(hKey, key, 0, &type, (LPBYTE)buf, &len);
	if (rc != ERROR_SUCCESS) {
		error(rc, "Failed to get registry %s\\%s. return %d", path, key, rc);
		RegCloseKey(hKey);
		return 1;
	}

	*value = buf;

	RegCloseKey(hKey);
	return 0;
}

#define DISABLED_ADAPTERS_REG_KEY	"prl_nettool_disabled"

static int save_adapter_disabled(const char *name, int enable)
{
	char *buf = NULL;
	char save_buf[4096];
	int rc, found;
	struct namelist *adapters = NULL;

	rc = get_reg_value(TOOL_GUEST_UTILS_REGISTRY_PATH, DISABLED_ADAPTERS_REG_KEY, &buf);
	//ignore error. reg path can be not found

	if  (rc == 0) {
		namelist_split_delim(&adapters, buf, ":");
		free(buf);
		buf = NULL;
	}

	found = namelist_search(name, &adapters);
	if (enable) {
		//enable adapter. need to remove mention of it
		if (!found) //nothing to do
			return 0;
		else
			namelist_remove(name, &adapters);
	} else {
		//disable adapter. need to save it
		if (found) //nothing to do
			return 0;
		else
			namelist_add(name, &adapters);
	}

	print_namelist_to_str_delim(&adapters, save_buf, sizeof(save_buf), ":");
	namelist_clean(&adapters);

	rc = set_reg_value(TOOL_GUEST_UTILS_REGISTRY_PATH, DISABLED_ADAPTERS_REG_KEY, save_buf);

	return rc;
}

int set_hostname(struct nettool_mac *params)
{
	char hostname[1024]; //to store value
	DNS_STATUS status;
	char netbios_name[MAX_COMPUTERNAME_LENGTH + 1];
	DWORD netbios_name_len = (DWORD)sizeof(netbios_name);
	char *computer_name = NULL;
	char *domain = NULL;
	const char *reg_hostname[] = {S_HOSTNAME_NV_REG_NAME,
		S_HOSTNAME_REG_NAME, NULL};
	const char *reg_domain[] = {S_DOMAIN_NV_REG_NAME,
		S_DOMAIN_REG_NAME, NULL};
	const char *reg_computer_name[] = {S_ACTIVE_NAME_REG_VALUE_KEY,
		S_COMPUTER_NAME_REG_VALUE_KEY, NULL};
	int ret, i = 0;
	int has_dot = 0;
	size_t len;

	if (params->value == NULL)
		return 0;

	hostname[0] = '\0';
	if (strlen(params->value) > 0) {
		strncpy(hostname, params->value, sizeof(hostname));
		hostname[sizeof(hostname)-1] = '\0';
	}

	if (strchr(hostname, '.') != NULL)
		has_dot = 1;

	computer_name = strtok(hostname, ".");
	if (computer_name == NULL)
		return 0;
	len = strlen(computer_name);
	/* strip trailing spaces, overwise Windows will have a bug with workgroup (PSBM-17913) */
	while (len > 0 && isspace(computer_name[len-1]))
		computer_name[--len] = 0;

	status = DnsValidateName_A(computer_name, DnsNameHostnameLabel);
	if (status != ERROR_SUCCESS && status != DNS_ERROR_NON_RFC_NAME) {
		error(status, "Hostname '%s' is invalid\n", computer_name);
		return status;
	}

	domain = strtok(NULL, ",");
	/* process "hostname." string as empty domain (PSBM-17894) */
	if (domain == NULL && has_dot)
		domain = "";

	/* set TCP properties first */
	/* set hostname in TCP properties in registry */
	for (i = 0; reg_hostname[i] != NULL; i++) {
		ret = set_reg_value(S_TCPPARAMS_REG_VALUE_KEY, reg_hostname[i], computer_name);
		if (ret)
			return ret;
	}
	/* set domain in TCP properties in registry */
	if (domain != NULL) {
		len = strlen(domain);
		/* strip trailing dot/space, overwise Windows will have a bug with workgroup (PSBM-17913) */
		while (len > 0 && (domain[len-1] == '.' || isspace(domain[len-1])))
			domain[--len] = 0;

		if (0 == len) {
			for (i = 0; reg_domain[i] != NULL; ++i) {
				set_reg_value(S_TCPPARAMS_REG_VALUE_KEY, reg_domain[i], "");
			}
		} else {
			status = DnsValidateName_A(domain, DnsNameDomain);
			if (status != ERROR_SUCCESS && status != DNS_ERROR_NON_RFC_NAME) {
				error(status, "Domain name '%s' is invalid\n", domain );
				return status;
			}

			for (i = 0; reg_domain[i] != NULL; i++) {
				ret = set_reg_value(S_TCPPARAMS_REG_VALUE_KEY, reg_domain[i], domain);
				if (ret)
					return ret;
			}
		}
	}

	/* set ComputerName properties (in upper case) */
	ret = DnsHostnameToComputerNameA(computer_name, netbios_name, &netbios_name_len);
	if (ret == 0) //DnsHostnameToComputerNameA return 0 if conversion fails.
		return 1;

	for (i = 0; reg_computer_name[i] !=  NULL; i++) {
		ret = set_reg_value(reg_computer_name[i], S_COMPUTER_REG_NAME, netbios_name);
		if (ret)
			return ret;
	}

	return 0;
}


int set_dhcp(struct netinfo *if_it, struct nettool_mac *params)
{
	int rc;
	struct nettool_mac *mac_it;

	if (strchr(params->value, '4'))
	{
		if (!if_it->configured_with_dhcp)
		{
			//switch to dynamic
			char str[1024];

			sprintf(str, "interface ip set dns \"%s\" dhcp", if_it->name);
			if ((rc = exec_netsh(str)))
				return rc;
			sprintf(str, "interface ip set address \"%s\" dhcp", if_it->name);
			if ((rc = exec_netsh(str)))
				return rc;

			if_it->configured_with_dhcp = 1;
		}
		else
			error(0, "DHCP v4 is already enabled");
	}

	mac_it = get_opt_mac(if_it->mac, NET_OPT_IP);
	if (strchr(params->value, '6') ||
		(mac_it == NULL && is_ipv6_supported()) /*ipv6 not specified at all*/ )
	{
		if (!if_it->configured_with_dhcpv6)
		{
			//switch to dynamic
			char str[1024];

			sprintf(str, "interface ipv6 set dns \"%s\" dhcp", if_it->name);
			if ((rc = exec_netsh(str)))
				return rc;

			//remove all IPs except of local-link
			if (remove_ipv6_ips(if_it))
					return -1;

			if_it->configured_with_dhcpv6 = 1;
		}
		else
			error(0, "DHCP v6 is already enabled");
	}

	return 0;
}

#if (NTDDI_VERSION < NTDDI_LONGHORN)

static int remove_route_xp(void)
{
	HKEY key;
	DWORD res, sz;
	char buf[MAX_PATH+1], str[MAX_PATH+1];
	char * param[4], *s;
	int i, j;
	int rc = 0, ret;

	res = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
			    "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\PersistentRoutes",
			    0, KEY_ENUMERATE_SUB_KEYS, &key);
	if (res != ERROR_SUCCESS) {
		if (res != ERROR_PATH_NOT_FOUND) {
			error(0, "RegOpenKeyEx() err %u", res);
			return res;
		}
		return 0;
	}

	for (i = 0; ; ++i) {

		sz = _countof(buf);
		res = RegEnumKeyExA(key, i, buf, &sz, 0, 0, 0, 0);
		if (res == ERROR_NO_MORE_ITEMS)
			break;
		if (res != ERROR_SUCCESS) {
			error(0, "RegEnumKeyEx() err %u", res);
			continue;
		}

		/* format: addr,mask,gw,metric */
		strcpy(str, buf);
		s = strtok(buf, ",");
		j = 0;
		while (s && j < _countof(param)) {
			param[j++] = s;
			s = strtok(NULL, ",");
		}
		if (j != _countof(param)) {
			error(0, "Bad PersistentRoutes entry %ws", buf);
			continue;
		}

		sprintf(buf, "route delete -p %s mask %s %s", param[0], param[1], param[2]);
		ret = exec_cmd(buf);
		if (ret != 0 && rc == 0)
			rc = ret;
	}

	RegCloseKey(key);
	return rc;
}

#else

static int exec_cmd_pipe_stdout(char * text, FILE ** filep)
{
	int rc = -1;
	HANDLE rd = NULL, wr = NULL;
	SECURITY_ATTRIBUTES attr;
	PROCESS_INFORMATION procinfo = { 0 };
	STARTUPINFOW startinfo = { 0 };
	WCHAR cmdline[MAX_PATH+1];
	int fd;

	debug("run: %s\n", text);

	attr.nLength = sizeof(attr);
	attr.bInheritHandle = TRUE;
	attr.lpSecurityDescriptor = NULL;
	if (!CreatePipe(&rd, &wr, &attr, 0)) {
		error(0, "CreatePipe() failed, err = %u", GetLastError());
		goto out;
	}
	SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

	startinfo.cb = sizeof(startinfo);
	startinfo.hStdInput = NULL;
	startinfo.hStdOutput = wr;
	startinfo.hStdError = NULL;
	startinfo.dwFlags = STARTF_USESTDHANDLES;

	_snwprintf(cmdline, MAX_PATH, L"%hs", text);
	cmdline[MAX_PATH] = 0;

	if (!CreateProcessW(NULL,
			    cmdline,		// command line
			    NULL,		// process security attributes
			    NULL,		// primary thread security attributes
			    TRUE,		// handles are inherited
			    CREATE_NO_WINDOW,	// creation flags
			    NULL,		// use parent's environment
			    NULL,		// use parent's current directory
			    &startinfo,		// STARTUPINFO pointer
			    &procinfo))		// receives PROCESS_INFORMATION
	{
		error(0, "CreateProcess(\"%hs\") failed, err = %u", text, GetLastError());
		goto out;
	}

	CloseHandle(wr);
	wr = NULL;

	fd = _open_osfhandle((intptr_t)rd, _O_RDONLY);
	if (fd < 0) {
		error(0, "_open_osfhandle() failed, errno = %u\n", errno);
		goto out;
	}
	// from now fclose() will do CloseHandle() as well
	rd = NULL;
	*filep = _fdopen(fd, "r");
	if (*filep == NULL) {
		error(0, "_open_osfhandle() failed, errno = %u\n", errno);
		_close(fd);
		goto out;
	}

	rc = 0;

out:
	if (procinfo.hProcess)
		CloseHandle(procinfo.hProcess);
	if (procinfo.hThread)
		CloseHandle(procinfo.hThread);
	if (rd)
		CloseHandle(rd);
	if (wr)
		CloseHandle(wr);

	return rc;
}

static int remove_route_vista(struct netinfo *if_it, const char *family)
{
	FILE * fd = NULL;
	char line[256], str[256], cmd[256];
	char * param[6], *s;
	int rc;
	int i, count, len;

	sprintf(cmd, "netsh interface %s show route store=persistent", family);

	rc = exec_cmd_pipe_stdout(cmd, &fd);
	if (rc != 0)
		return rc;

	line[sizeof(line)-1] = 0;
	count = 0;
	while (fgets(line, sizeof(line)-1, fd) != NULL) {

		/* skip three lines (header) */
		if (++count <= 3)
			continue;

		/* trim spaces and newlines */
		len = strlen(line);
		while (len > 0 && isspace(line[len-1]))
			line[--len] = 0;

		/* format: Publish Type Metric Prefix Idx InterfaceName */
		strcpy(str, line);
		s = strtok(line, " \t");
		i = 0;
		while (s && i < _countof(param)) {
			param[i++] = s;
			s = strtok(NULL, " \t");
		}
		if (i != _countof(param))
			continue;

		/* a tricky part: InterfaceName may also contain spaces trimmed by strtok -
		   so use a pointer from origin string */
		param[_countof(param) - 1] = str + (param[_countof(param) - 1] - line);

		//for (i=0; i < _countof(param); ++i)
		//	printf("i=%d [%s]\n", i, param[i]);

		if (!_stricmp(if_it->name, param[5])) {
				sprintf(cmd, "interface %s delete route %s \"%s\" store=persistent",
					family, param[3], if_it->name);
				exec_netsh(cmd);
		}
	}

	fclose(fd);
	return 0;
}

#endif

int set_route(struct netinfo *if_it, struct nettool_mac *params)
{
	char str[1024];
	struct namelist *gws = NULL;
	struct namelist *gw;
	int rc;

#if (NTDDI_VERSION < NTDDI_LONGHORN)
	char if_ip[100];
	rc = find_if_first_ip(if_it, if_ip);
	if (rc < 0)
		return rc;
#endif

	//split params->value
	namelist_split(&gws, params->value);

	//for each ip do
	gw = gws;
	while (gw)
	{
		if (!strncmp(gw->name, NET_STR_OPT_REMOVE, strlen(NET_STR_OPT_REMOVE)))
		{
#if (NTDDI_VERSION < NTDDI_LONGHORN)
			rc = remove_route_xp();
#else
			rc = remove_route_vista(if_it, "ipv4");
			rc = remove_route_vista(if_it, "ipv6");
#endif
		} else {
			struct route route = {NULL, NULL, NULL};
			parse_route(gw->name, &route);
			if (is_ipv6(gw->name)) {

				sprintf(str, "interface ipv6 add route %s%s \"%s\" %s %s%s store=persistent",
					route.ip, strchr(route.ip, '/') ? "" : "/128", if_it->name,
					route.gw ? route.gw : "", route.metric ? "metric=" : "",
					route.metric ? route.metric : "");
				rc = exec_netsh(str);
			} else {
#if (NTDDI_VERSION < NTDDI_LONGHORN)
				char ip[100];
				char mask[IP_LENGTH+1];
				if (split_ip_mask(route.ip, ip, mask) < 0) {
					strcpy(ip, route.ip);
					strcpy(mask, "255.255.255.255");
				}
				char *hop = route.gw ? route.gw : if_ip;
				sprintf(str, "route add -p %s mask %s %s %s %s", ip, mask, hop,
						route.metric ? "metric" : "", route.metric ? route.metric : "");
				rc = exec_cmd(str);
#else
				sprintf(str, "interface ipv4 add route %s%s \"%s\" %s %s%s store=persistent",
					route.ip, strchr(route.ip, '/') ? "" : "/32", if_it->name,
					route.gw ? route.gw : "", route.metric ? "metric=" : "",
					route.metric ? route.metric : "");
				rc = exec_netsh(str);
#endif
			}
			clear_route(&route);
		}

		gw = gw->next;
	}

	rc = 0;
	return rc;
}

int clean(struct netinfo *if_it)
{
	VARUNUSED(if_it);
	return 0;
}

static int enable_adapter_by_name(const char *name, int enable)
{
	char str[1024];
	int rc = 0;

	if (!enable) //save disabled
		save_adapter_disabled(name, 0);

	sprintf(str, "interface set interface \"%s\" %s", name, enable ? "ENABLE" : "DISABLE");
	rc = exec_netsh(str);

	if (enable //save enabled
		&& (rc == 0 || rc == 1)) //during windows shutdown (#PSBM-9930)
								//netsh return STATUS_DLL_INIT_FAILED
								//in such case we should not clean registry key
								//allow only success or param error
		save_adapter_disabled(name, 1);

	return 0;
}

int enable_adapter(struct netinfo *if_it, int enable)
{
	return enable_adapter_by_name(if_it->name, enable);
}

int restore_adapter_state()
{
	char *buf;
	int rc, found = 0;
	struct namelist *adapters = NULL, *it;

	rc = get_reg_value(TOOL_GUEST_UTILS_REGISTRY_PATH, DISABLED_ADAPTERS_REG_KEY, &buf);
	if (rc > 0) //some error
		return 1;
	if (rc < 0 || buf == NULL || strlen(buf) == 0) //no state
		return 0;

	//error(0, "value '%s'", buf);
	namelist_split_delim(&adapters, buf, ":");
	free(buf);

	it = adapters;
	while(it) {
		found = 1;
		//error(0, "Restore '%s'", it->name );
		enable_adapter_by_name(it->name, 1);
		it = it->next;
	}

	if (found)
		wait_for_start(adapters);

	namelist_clean(&adapters);

	return rc;
}

int enable_disabled_adapter(const char *mac)
{
	char buf[MAC_LENGTH+1];
	wchar_t *uuid_start;
	char uuid[NAME_LENGTH];
	char name[NAME_LENGTH];
	int found = 0;
	DWORD dwSize = 0;
	DWORD dwRetVal = 0;
	unsigned int i;

	// variables used for GetIfTable and GetIfEntry
	MIB_IFTABLE *pIfTable;
	MIB_IFROW *pIfRow;

	// Allocate memory for our pointers.
	pIfTable = (MIB_IFTABLE *) MALLOC(sizeof (MIB_IFTABLE));
	if (pIfTable == NULL) {
		error(0, "Error allocating memory needed to call GetIfTable");
		return -1;
	}
	// Make an initial call to GetIfTable to get the
	// necessary size into dwSize
	dwSize = sizeof (MIB_IFTABLE);
	if (GetIfTable(pIfTable, &dwSize, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
		FREE(pIfTable);
		pIfTable = (MIB_IFTABLE *) MALLOC(dwSize);
		if (pIfTable == NULL) {
			error(0, "Error allocating memory needed to call GetIfTable");
			return -1;
		}
	}
	// Make a second call to GetIfTable to get the actual
	// data we want.
	if ((dwRetVal = GetIfTable(pIfTable, &dwSize, FALSE)) == NO_ERROR) {

		for (i = 0; i < pIfTable->dwNumEntries; i++)
		{
			pIfRow = (MIB_IFROW *) & pIfTable->table[i];

			if (pIfRow->dwType != IF_TYPE_ETHERNET_CSMACD)
				continue;

			//printf("\tInterfaceName[%d]:\t %ws\n", i, pIfRow->wszName);

			if (mac_to_str(pIfRow->bPhysAddr, pIfRow->dwPhysAddrLen,
				buf, sizeof(buf)) == NULL)
				return -1;

			//printf("\tMAC[%d]:\t %s\n", i, buf);

			if (strcmp(buf, mac))
				continue;

			//skip enabled adapters
			if (pIfRow->dwOperStatus != IF_OPER_STATUS_NON_OPERATIONAL)
				continue;

			//found!!!
			//pIfRow->wszName has name \DEVICE\TCPIP_{E0854051-CFD5-4D6A-86F3-9C971C7CFE7A}
			//get only uuid
			if (wcslen(pIfRow->wszName) < 20)
			{
				//strange name of device
				error(0, "Error: Strange name of device '%wS'", pIfRow->wszName);
				continue;
			}

			uuid_start = pIfRow->wszName + strlen("\\DEVICE\\TCPIP_");
			//convert wchar to char
			snprintf(uuid, sizeof(uuid), "%wS", uuid_start);

			//get friendly name of interface like "Local Area Connection"
			if (getNetInterfaceName(uuid, name, sizeof(name)))
				return -1;

			found = 1;
			enable_adapter_by_name(name, 1);
		}
	} else {
		error(0, "GetIfTable failed with error: \n", dwRetVal);
		if (pIfTable != NULL) {
			FREE(pIfTable);
			pIfTable = NULL;
		}
		return -1;
	}
	if (pIfTable != NULL) {
		FREE(pIfTable);
		pIfTable = NULL;
	}
	return found;
}


int restart_guest_network()
{
	error(0, "ERROR: restart network not implemented for Windows");
	return -1;
}

