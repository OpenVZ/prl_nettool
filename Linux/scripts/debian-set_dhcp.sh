#!/bin/bash
# Copyright (c) 2015-2017, Parallels International GmbH
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


if [ -f $NWSYSTEMCONF -o -f $NMCONFFILE ]; then
	if [ ! -f "${NWMANAGER}" ] ; then
		echo "Network manager ${NWMANAGER} not found"
		exit 3
	fi

	ret=0
	for dev in ${ETH_DEV} "${ETH_DEV}:[0-9]+"; do
		remove_debian_interfaces $dev
		[ $ret -eq 0 ] && ret=$?
	done

	[ $ret -ne 0 ] && restart_nm_wait

	clean_nm_connections $ETH_DEV $ETH_MAC $ETH_MAC_NW

	echo "[connection]
id=$ETH_DEV
uuid=`generate_uuid`
type=802-3-ethernet
autoconnect=true
timestamp=0
" > $NWSYSTEMCONNECTIONS/$ETH_DEV

	if [ "x$PROTO4" == "xyes" ] ; then
		echo "
[ipv4]
method=auto
ignore-auto-routes=false
ignore-auto-dns=false
never-default=false
" >> $NWSYSTEMCONNECTIONS/$ETH_DEV
	fi

	if [ "x$PROTO6" == "xyes" ] ; then
		echo "
[ipv6]
method=auto
ignore-auto-routes=false
ignore-auto-dns=false
never-default=false
" >> $NWSYSTEMCONNECTIONS/$ETH_DEV
	fi

	echo "
[802-3-ethernet]
speed=0
duplex=full
auto-negotiate=true
mac-address=$ETH_MAC_NW
mtu=0" >> $NWSYSTEMCONNECTIONS/$ETH_DEV

	chmod 0600 $NWSYSTEMCONNECTIONS/$ETH_DEV

	nm_connection_reload ${ETH_DEV}
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

$path/debian-restart.sh ${ETH_DEV}

exit 0
# end of script
