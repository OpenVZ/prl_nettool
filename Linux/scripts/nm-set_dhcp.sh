#!/bin/bash
# Copyright (C) Parallels, 1999-2009. All rights reserved.
#
# This script configure adapter to use DHCP inside RedHat like VM.
#
# Parameters: <dev> <mac>
#   <dev>         - name of device. (example: eth2)
#   <mac>         - hardware address of device
#

prog="$0"
path="${prog%/*}"
funcs="$path/functions"

if [ -f "$funcs" ] ; then
	. $funcs
else
	echo "Program $0"
	echo "'$funcs' was not found"
	exit 1
fi

ETH_DEV=$1
ETH_MAC=$2
PROTO=$3

PROTO4="manual"
PROTO6="manual"
CLEAN4="no"
CLEAN6="no"

uuid=`nm_check_and_create $ETH_DEV $ETH_MAC` ||
	exit $?

for proto in ${PROTO}; do
	if [ "x$proto" == "x4" ] ; then
		PROTO4="auto"
		CLEAN4="yes"
	elif [ "x$proto" == "x6" ] ; then
		PROTO6="auto"
		CLEAN6="yes"
	fi
done

call_nmcli c modify $uuid ipv4.method $PROTO4
if [ $? -ne 0 ]; then
	call_nmcli c modify $uuid ipv4.method link-local ||
		exit $?
fi

if [ $CLEAN4 == "yes" ] ; then
	# clean IPv4 addreses and gw from connection
	call_nmcli c modify $uuid ipv4.gateway ""
	call_nmcli c modify $uuid ipv4.address ""
fi

call_nmcli c modify $uuid ipv6.method $PROTO6
if [ $? -ne 0 ]; then
	call_nmcli c modify $uuid ipv6.method link-local ||
		exit $ret
fi

if [ $CLEAN6 == "yes" ] ; then
	# clean IPv6 addreses and gw from connection
	call_nmcli c modify $uuid ipv6.gateway ""
	call_nmcli c modify $uuid ipv6.address ""
fi

call_nmcli c up $uuid || exit $?
# end of script
