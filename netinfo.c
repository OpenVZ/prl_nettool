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
 * methods for getting network parameters
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <iphlpapi.h>
#include <stdlib.h>
#include <initguid.h>
#include <devguid.h>
#include <objbase.h>

#include "common.h"
#include "netinfo.h"
#include "namelist.h"


int read_dns(DWORD ad_index, struct netinfo *netinfo)
{
	int err;
	IP_PER_ADAPTER_INFO *buf = NULL;
	ULONG len = 0;
	int repeats = 0;
	//dns server - common
	//use GetPerAdapterInfo

	err = GetPerAdapterInfo(ad_index, buf, &len);
	while ( err == ERROR_BUFFER_OVERFLOW && repeats < 10 )
	{
		if (buf != NULL)
			FREE( buf );

		buf = (IP_PER_ADAPTER_INFO *) MALLOC(len);
		if (buf == NULL) {
			error(errno, "can't allocate memory for IP_PER_ADAPTER_INFO");
			return -1;
		}
		err = GetPerAdapterInfo(ad_index, buf, &len);
		repeats ++;
	}

	if ( err == ERROR_SUCCESS )
	{
		IP_ADDR_STRING *dns_ip = &(buf->DnsServerList);
		while(dns_ip){
			namelist_add(dns_ip->IpAddress.String, &(netinfo->dns));
			dns_ip = dns_ip->Next;
		}
	}
	FREE( buf );

	return 0;
}


int read_searchdomain(struct netinfo **netinfo_head)
{
	//domain search list
	//Win API specified only for Vista (struct IP_ADAPTER_ADDRESSES)
	//so, read registry

	char domains[1024]; //to store value
	DWORD rc, dwType;
	HKEY hKey;
	struct netinfo *info = NULL;
	ULONG len;
	char * parser;

	len = sizeof(domains);

	rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, S_TCPPARAMS_REG_VALUE_KEY, 0, KEY_READ, &hKey);

	if (rc != ERROR_SUCCESS){
		error(errno, "Can't open registry %s\n", S_TCPPARAMS_REG_VALUE_KEY);
		return 1;
	}

	rc = RegQueryValueExA(hKey, S_SEARCHLIST_REG_NAME, NULL, &dwType, (LPBYTE)domains, &len);
	if (rc != ERROR_SUCCESS){
		error(errno, "Can't read registry %s\n", S_TCPPARAMS_REG_VALUE_KEY);
		RegCloseKey(hKey);
		return 1;
	}

	if (dwType != REG_SZ){
		error(errno, "Wrong type of %s\n", S_SEARCHLIST_REG_NAME);
		RegCloseKey(hKey);
		return 1;
	}

	rc = RegCloseKey(hKey);

	if (strlen(domains) == 0)
		return 0;

	info = netinfo_get_first(netinfo_head);
	if (info == NULL)
		return 1;

	parser=strtok(domains, ",");
	while(parser) {
		namelist_add(parser, &(info->search));
		parser=strtok(NULL, ",");
	}

	return 0;
}

#define STR_SIZE	4000

int getNetInterfaceName( const char *pAdapterGuid, char* adapterName, int size)
{
	int rc=0;

	char szKey[STR_SIZE+1];
	WCHAR wcGuid[STR_SIZE+1];
	char cGuid[STR_SIZE+1];
	HKEY hKey;
	DWORD dwType;
	DWORD rSize = size;
	size_t converted = 0;

	if (!StringFromGUID2( &GUID_DEVCLASS_NET, wcGuid, sizeof(wcGuid) ))
	{
		error(0 ,"Error translate GUID");
		return E_FAIL;
	}

	wcstombs_s(&converted, cGuid, sizeof(wcGuid),
					wcGuid, sizeof(wcGuid));

	snprintf(szKey, STR_SIZE,  "%s\\%s\\%s\\%s", S_NETWORK_REG_KEY, cGuid, pAdapterGuid, S_CONNECTION_REG_KEY);

	rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, szKey, 0, KEY_READ, &hKey);

	// Open registry entry under HKEY_LOCAL_MACHINE.
	if (rc != ERROR_SUCCESS)
	{
		error(rc, "Error open registry: %s", szKey);
		return rc;
	}

	// Read registry value for checking existance of parameter.
	rc = RegQueryValueExA(hKey, S_NAME_REG_VALUE_NAME, NULL, &dwType,
		(LPBYTE)adapterName, &rSize);
	if (rc != ERROR_SUCCESS)
	{
		error(rc, "Error in work with registry: %u, size = %u", rc, rSize);
		RegCloseKey(hKey);
		return rc;
	}

	adapterName[size - 1] = 0;

	return rc;
}


int read_ifconf(struct netinfo **netinfo_head)
{
    /* Declare and initialize variables */
	int rc = NO_ERROR;

// It is possible for an adapter to have multiple
// IPv4 addresses, gateways.

    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter = NULL;
	struct netinfo *if_it = NULL;
	char buf[MAC_LENGTH+1];

    ULONG ulOutBufLen = sizeof (IP_ADAPTER_INFO);
    pAdapterInfo = (IP_ADAPTER_INFO *) MALLOC(sizeof (IP_ADAPTER_INFO));
    if (pAdapterInfo == NULL) {
        error(errno, "Error allocating memory needed to call GetAdaptersinfo");
        return ERROR_NOT_ENOUGH_MEMORY;
    }
// Make an initial call to GetAdaptersInfo to get
// the necessary size into the ulOutBufLen variable
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        FREE(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO *) MALLOC(ulOutBufLen);
        if (pAdapterInfo == NULL) {
			error(errno, "Error allocating memory needed to call GetAdaptersinfo");
            return ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    if ((rc = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) != NO_ERROR)
	{
		error(0, "GetAdaptersInfo failed with error: %d\n", rc);
	    FREE(pAdapterInfo);
		return rc;
	}

    pAdapter = pAdapterInfo;
    while (pAdapter) {
		IP_ADDR_STRING *pIpAddr = NULL;

		if (pAdapter->Type != MIB_IF_TYPE_ETHERNET)
			goto metka_next_adapter;

		if_it = netinfo_new();
		if (if_it == NULL) {
			error(0, "Error allocating memory for netinfo");
			return ERROR_NOT_ENOUGH_MEMORY;
		}

		if (mac_to_str(pAdapter->Address, pAdapter->AddressLength,
			       buf, MAC_LENGTH+1) == NULL) {
			netinfo_free(if_it);
			return ERROR_INVALID_PARAMETER;
		}

		strcpy(if_it->mac, buf);

		if ((rc = getNetInterfaceName(pAdapter->AdapterName, if_it->name, sizeof(if_it->name)))) {
			netinfo_free(if_it);
			return rc;
		}

		pIpAddr = &(pAdapter->IpAddressList);
		while(pIpAddr){
			char ip_mask[100];

			sprintf(ip_mask, "%s/%s",pIpAddr->IpAddress.String, pIpAddr->IpMask.String);
			namelist_add(ip_mask, &(if_it->ip));
			pIpAddr = pIpAddr->Next;
		}

		if (pAdapter->GatewayList.IpAddress.String)
			namelist_add(pAdapter->GatewayList.IpAddress.String, &(if_it->gateway));

		if (pAdapter->DhcpEnabled) {
			if_it->configured_with_dhcp = 1;
		} else {
			if_it->configured_with_dhcp = 0;
		}

		read_dns(pAdapter->Index, if_it);

		netinfo_add(if_it, netinfo_head);

metka_next_adapter:
        pAdapter = pAdapter->Next;
    }

    FREE(pAdapterInfo);

    return rc;
}


#ifndef NTDDI_VERSION
#error "NTDDI_VERSION is not defined"
#endif

#if (NTDDI_VERSION >= NTDDI_LONGHORN)


int socket_address_to_str(SOCKET_ADDRESS *saddr, char *buf, size_t size)
{
	LPSOCKADDR addr;

	if (saddr == NULL)
	{
		error(0, "socket_address is not defined");
		return -1;
	}

	addr = saddr->lpSockaddr;

	if (saddr->iSockaddrLength == 0 || addr == NULL)
	{
		error(0, "socket_address is empty");
		return -1;
	}

	if (inet_ntop(addr->sa_family, INETADDR_ADDRESS(addr), buf, size) == NULL)
	{
		error(0, "Failed to convert ipv socket_address to string");
		return -1;
	}

	return 0;
}

void print_socket_address(SOCKET_ADDRESS *saddr)
{
	char buf[100];
	int rc = socket_address_to_str(saddr, buf, sizeof(buf));
	if (rc == 0)
		printf("addr: %s\n", buf);
}

int read_ifconf_vista(int fam, struct netinfo **netinfo_head)
{
    DWORD dwSize = 0;
    int rc = NO_ERROR;

    // Set the flags to pass to GetAdaptersAddresses
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS;

    // default to unspecified address family (both)
    ULONG family = AF_UNSPEC;

    LPVOID lpMsgBuf = NULL;

    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    ULONG outBufLen = 0;

    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
    PIP_ADAPTER_UNICAST_ADDRESS_LH pUnicast = NULL;
    PIP_ADAPTER_ANYCAST_ADDRESS_XP pAnycast = NULL;
    PIP_ADAPTER_MULTICAST_ADDRESS_XP pMulticast = NULL;
    PIP_ADAPTER_DNS_SERVER_ADDRESS_XP pDnServer = NULL;
    PIP_ADAPTER_PREFIX_XP pPrefix = NULL;
	//PIP_ADAPTER_DNS_SUFFIX pDnsSuffix = NULL;
  	struct netinfo *if_it = NULL;
	char mac_buf[MAC_LENGTH+1];

    if (fam == 4)
        family = AF_INET;
    else if (fam == 6)
        family = AF_INET6;

    for(outBufLen = sizeof (IP_ADAPTER_ADDRESSES);;)
    {
        pAddresses = (IP_ADAPTER_ADDRESSES *) MALLOC(outBufLen);
        if (pAddresses == NULL)
	{
            error(0, "Memory allocation failed for IP_ADAPTER_ADDRESSES struct");
            return ERROR_NOT_ENOUGH_MEMORY;
        }
        rc = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);
        if (NO_ERROR == rc)
            break;

        FREE(pAddresses);
        if (ERROR_BUFFER_OVERFLOW != rc)
        {
            error(rc, "Call to GetAdaptersAddresses failed with error: %d", rc);
	    return rc;
        }
    }
    for (pCurrAddresses = pAddresses; pCurrAddresses != NULL;
				pCurrAddresses = pCurrAddresses->Next)
	{
		BOOL need_list_add = FALSE;

		if (pCurrAddresses->IfType != IF_TYPE_ETHERNET_CSMACD)
			continue;

		if (mac_to_str(pCurrAddresses->PhysicalAddress, pCurrAddresses->PhysicalAddressLength,
						mac_buf, MAC_LENGTH+1) == NULL)
			return ERROR_INVALID_PARAMETER;

		if_it = netinfo_search_mac( netinfo_head, mac_buf);
		if (if_it == NULL)
		{
			if_it = netinfo_new();
			if (if_it == NULL) {
				error(0, "Error allocating memory for netinfo");
				return ERROR_NOT_ENOUGH_MEMORY;
			}
			need_list_add = TRUE;
			strcpy(if_it->mac, mac_buf);
		}

		if (pCurrAddresses->Flags & IP_ADAPTER_DHCP_ENABLED)
			if_it->configured_with_dhcp = 1;
		else
			if_it->configured_with_dhcp = 0;

		if(AF_INET6 == family)
		{
			//by default think that adapter ipv6 is automatically configured
			if_it->configured_with_dhcpv6 = 1;
		}

		for (pUnicast = pCurrAddresses->FirstUnicastAddress;
					pUnicast != NULL; pUnicast = pUnicast->Next)
		{
			char ip_mask[100], ip[100], mask[100];
			int is_manual = 0;
			if (socket_address_to_str(&(pUnicast->Address), ip, sizeof(ip)) < 0) {
				netinfo_free(if_it);
				return ERROR_INVALID_PARAMETER;
			}

			if (AF_INET == family)
			{
				unsigned int bmask = 0xFFFFFFFF >> (32 - pUnicast->OnLinkPrefixLength);
				inet_ntop(AF_INET, (void *) &bmask, mask, sizeof(mask));
			}
			else
			{
				sprintf(mask, "%d", pUnicast->OnLinkPrefixLength);
				if (IpSuffixOriginManual == pUnicast->SuffixOrigin)
				{
					//exist manual IP, mark dhcp - off
					if_it->configured_with_dhcpv6 = 0;
					is_manual = 1;
				}

			}

			sprintf(ip_mask, "%s/%s", ip, mask);
			namelist_add(ip_mask, &(if_it->ip));
			if (AF_INET6 == family && !is_manual)
				namelist_add(ip_mask, &(if_it->ip_link));
		}

		for (pDnServer = pCurrAddresses->FirstDnsServerAddress;
					pDnServer != NULL; pDnServer = pDnServer->Next)
		{
			char ip[100];

			if (socket_address_to_str(&(pDnServer->Address), ip, sizeof(ip)) < 0) {
				netinfo_free(if_it);
				return -1;
			}

			namelist_add(ip, &(if_it->dns));
		}

		sprintf(if_it->name, "%wS", pCurrAddresses->FriendlyName);

		if (pCurrAddresses->FirstGatewayAddress != NULL)
		{
			char ip[100];
			if (socket_address_to_str(&(pCurrAddresses->FirstGatewayAddress->Address), ip, sizeof(ip)) < 0) {
				netinfo_free(if_it);
				return ERROR_INVALID_PARAMETER;
			}

			namelist_add(ip, &(if_it->gateway));
		}

		if (need_list_add)
			netinfo_add(if_it, netinfo_head);
    }

    FREE(pAddresses);
    return rc;
}

#endif

void wait_for_start(const struct namelist *adapters)
{
	int enable_count;
	struct netinfo *netinfo_head;
	const struct namelist *it;

	netinfo_head = NULL;

	for(enable_count=0; enable_count <= 5*60; enable_count++) // wait up to 5 minute
	{
		int all_enabled = 1;
		int rc;

		if (enable_count)
			Sleep(1000);

		//reload information in system
		rc = get_device_list(&netinfo_head);
		switch (rc)
		{
			// Wait for non-permanent errors to disappear:
			//  ERROR_NO_DATA from GetAdaptersInfo()
			//	network device list is not available yet
			//  ERROR_FILE_NOT_FOUND from getNetInterfaceName()
			//	interface is not yet completed pnp configuration
		case ERROR_NO_DATA:
		case ERROR_FILE_NOT_FOUND:
			continue;
		default:
			error(rc, "get_device_list failed");
			return;
		}

		//check enabled adapter
		it = adapters;
		while(it != NULL)
		{
			if (netinfo_search_mac(&netinfo_head, it->name) == NULL
				&& netinfo_search_name(&netinfo_head, it->name) == NULL)
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

int get_device_list(struct netinfo **netinfo_head) {
	int rc;
#if (NTDDI_VERSION >= NTDDI_LONGHORN)
	rc = read_ifconf_vista(6, netinfo_head);
	//	error(0 ,"\n=============================================\n");
	rc = read_ifconf_vista(4, netinfo_head);
#else
	rc = read_ifconf(netinfo_head);
#endif
	read_searchdomain(netinfo_head);

	return rc;
}



