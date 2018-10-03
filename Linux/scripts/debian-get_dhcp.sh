#!/bin/bash
# Copyright (c) 2015-2017, Parallels International GmbH
#
# This script detect is DHCP enabled for Debian like VMs.
#
# Arguments: <ADDR> <DEV> <PROTO>
#   ADDR       - hardware address of adapter
#   DEV        - name of device ( eth1 )
#   PROTO      - proto "4" or "6". If empty - 4.
#
# Return:
#   2 - can't detect or some error
#   0 - enabled
#   1 - disabled 
#

ETH_MAC=$1
ETH_DEV=$2
PROTO=$3
ETH_MAC_NW=`echo $ETH_MAC | sed "s,00,0,g"`

prog="$0"
path="${prog%/*}"
funcs="$path/functions"

if [ -f "$funcs" ] ; then
	. $funcs
else
	echo "Program $0"
	echo "File $funcs not found"
	exit 2
fi

nmscript=$path/nm-get_dhcp.sh

if is_netplan_controlled; then
	$path/$NETPLAN_CFG -a "get_dhcp" -d "$ETH_DEV" -p "$PROTO"
	exit $?
fi

is_nm_active
if [ $? -eq 0 -a -f $nmscript ]; then
	$nmscript "$@"
	exit $?
else

	if [ "x${PROTO}" == "x6" ] ; then
		CONFIGFILE="/etc/default/wide-dhcpv6-client"

		# config was not found
		[ -f "${CONFIGFILE}" ] || exit 1

		cat $CONFIGFILE | grep "^[[:space:]]*INTERFACES.*$ETH_DEV" >/dev/null 2>&1
	else
		# config was not found
		[ -f "${DEBIAN_CONFIGFILE}" ] || exit 2

		inet="inet"

		cat $DEBIAN_CONFIGFILE | grep "^[[:space:]]*iface $ETH_DEV $inet" | grep dhcp >/dev/null 2>&1
	fi

	[ $? -eq 0 ] && exit 0 || exit 1
fi
