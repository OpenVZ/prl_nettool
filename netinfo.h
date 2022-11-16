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
 * methods for getting network parameters
 */

#ifndef __NETINFO_H__
#define __NETINFO_H__

#ifdef _MAC_
#include <SystemConfiguration/SCPreferences.h>
#include <SystemConfiguration/SCNetworkConfiguration.h>
#endif

#include <stddef.h>

#define MAC_LENGTH	17 //len of XX:XX:XX:XX:XX:XX
#define IP_LENGTH	15 //len of XXX.XXX.XXX.XXX
#define NAME_LENGTH	260

struct netinfo
{
	char mac[MAC_LENGTH+1];
	char name[NAME_LENGTH];
	int idx; // windows adapter idx to be used with netsh
	struct namelist *ip, *search, *dns;
	struct namelist *ip_link; //link-local, site-local
	struct namelist *gateway;
	int configured_with_dhcp; // 1 - true. 0 - false
	int configured_with_dhcpv6;
	int disabled;
	int changed;
	struct netinfo *next;

};

int get_device_list(struct netinfo **netinfo);
struct netinfo *netinfo_search_mac(struct netinfo **netinfo_head, const char *mac);

void  netinfo_add(struct netinfo *if_info, struct netinfo **netinfo_head);
struct netinfo *netinfo_new(void);
void netinfo_free(struct netinfo *if_info);
struct netinfo *netinfo_get_first(struct netinfo **netinfo_head);
struct netinfo *netinfo_search_mac(struct netinfo **netinfo_head, const char *mac);
struct netinfo *netinfo_search_name(struct netinfo **netinfo_head, const char *name);
struct netinfo *netinfo_search_idx(struct netinfo **netinfo_head, int idx);
void netinfo_clean(struct netinfo **netinfo_head);
const char *mac_to_str(unsigned char *addr, size_t alen, char *buf, size_t blen);
int split_ip_mask(const char *ip_mask, char* ip, char *mask);
void wait_for_start(const struct namelist *adapters);

#ifdef _WIN_

#define S_NETWORK_REG_KEY  "SYSTEM\\CurrentControlSet\\Control\\Network"
#define S_CONNECTION_REG_KEY  "Connection"
#define S_NAME_REG_VALUE_NAME  "Name"

#define S_SEARCHLIST_REG_NAME  "SearchList"
#define S_HOSTNAME_NV_REG_NAME  "NV Hostname"
#define S_HOSTNAME_REG_NAME  "Hostname"
#define S_DOMAIN_NV_REG_NAME  "NV Domain"
#define S_DOMAIN_REG_NAME  "Domain"
#define S_TCPPARAMS_REG_VALUE_KEY  "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"

#define S_COMPUTER_REG_NAME  "ComputerName"
#define S_ACTIVE_NAME_REG_VALUE_KEY "SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ActiveComputerName"
#define S_COMPUTER_NAME_REG_VALUE_KEY "SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ComputerName"

int getNetInterfaceName(const char *pAdapterGuid, char* adapterName, int size);

#endif

#ifdef _MAC_

extern SCPreferencesRef SystemConfPrefs;
extern CFArrayRef SystemConfNetworkServices;

int OpenPrefs();
void ClosePrefs();

/// return true if v is valid and is a reference to typeId
int SCCheckReference( const void *v, CFTypeID typeId);
CFDictionaryRef DictionaryGetRecursive(CFDictionaryRef dict, CFStringRef s, ...);

Boolean
DictionarySetRecursive(CFDictionaryRef dict, const void *v, CFStringRef s, ...);

// Return Path in the preferences for the given service and entity.
// Returned value should be released.
CFStringRef getServiceEntityPath(SCNetworkServiceRef service, CFStringRef entity);


// converts CFString to ascii
void CFStringToAscii(CFStringRef src, char *dst, int size);

// returns BSD-name of the interface that is related to service.
CFStringRef getIfaceBsdNameForService(SCPreferencesRef prefs, SCNetworkServiceRef service);

// search service in SystemConfNetworkServices
SCNetworkServiceRef get_service_by_name(char *if_name);

#endif

#ifdef _LIN_

void detect_distribution();

#endif

#endif // __NETINFO_H__
