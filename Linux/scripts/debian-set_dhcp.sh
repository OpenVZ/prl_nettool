#!/bin/bash
# Copyright (c) 2015-2017, Parallels International GmbH
# Copyright (c) 2017-2019 Virtuozzo International GmbH. All rights reserved.
#
# This script configure adapter to use DHCP inside Debian like VM.
#
# Parameters: <dev> <MAC_addr>
#   <dev>         - name of device. (example: eth2)
#   <MAC_addr>    - MAC address of device
#

prog="$0"
path="${prog%/*}"
funcs="$path/functions"

if [ -f "$funcs" ] ; then
	. $funcs
else
	echo "Program $0"
	echo "File $funcs not found"
	exit 1
fi

ETH_DEV=$1
ETH_MAC=$2
PROTO=$3
ETH_MAC_NW=`echo $ETH_MAC | sed "s,00,0,g"`

PROTO4="no"
PROTO6="no"

for proto in ${PROTO}; do
	if [ "x$proto" == "x4" ] ; then
		PROTO4="yes"
	elif [ "x$proto" == "x6" ] ; then
		PROTO6="yes"
	fi
done

if is_netplan_controlled; then
	$path/$NETPLAN_CFG -a "set_dhcp" -d "$ETH_DEV" -p "$PROTO"
	exit $?
elif [ -f $NWSYSTEMCONF -o -f $NMCONFFILE ]; then
	call_nm_script $0 "$@"
	exit $?
else
	remove_debian_interfaces "${ETH_DEV}:[0-9]+"
	remove_debian_interfaces ${ETH_DEV}

	echo "allow-hotplug ${ETH_DEV}" >> $DEBIAN_CONFIGFILE
	echo "auto ${ETH_DEV}" >> $DEBIAN_CONFIGFILE

	if [ "x$PROTO4" == "xyes" ] ; then
		echo >> $DEBIAN_CONFIGFILE
		echo "iface ${ETH_DEV} inet dhcp" >> $DEBIAN_CONFIGFILE
		# 2.6.35 kernel doesn't flush IPv6 addresses
		echo "	pre-down ip -6 addr flush dev ${ETH_DEV} scope global || :" >> $DEBIAN_CONFIGFILE
		echo >> $DEBIAN_CONFIGFILE
	fi

	if [ "x$PROTO6" == "xyes" ] ; then
		#don't support dhcpv6 by config
		set_wide_dhcpv6 ${ETH_DEV}
	fi

fi

if [ "x$PROTO4" == "xyes" ] ; then
	#clean old IPv4
	ip -4 addr flush dev ${ETH_DEV}
fi

exit 0
# end of script
