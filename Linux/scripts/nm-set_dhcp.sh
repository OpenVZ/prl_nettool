#!/bin/bash
# Copyright (c) 1999-2017, Parallels International GmbH
# Copyright (c) 2017-2019 Virtuozzo International GmbH. All rights reserved.
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

uuid=`nm_check_and_create $ETH_DEV $ETH_MAC` ||
	exit $?

for proto in ${PROTO}; do
	if [ "x$proto" == "x4" ] ; then
		PROTO4="auto"
	elif [ "x$proto" == "x6" ] ; then
		PROTO6="auto"
	fi
done

[ $PROTO4 == "auto" -a $PROTO6 != "auto" ] && PROTO6="ignore"

call_nmcli c modify $uuid ipv4.method $PROTO4
if [ $? -ne 0 ]; then
	call_nmcli c modify $uuid ipv4.method link-local ||
		exit $?
fi

[ $PROTO4 == "auto" ] && nmcli_clean_ip_and_gw $uuid 4

call_nmcli c modify $uuid ipv6.method $PROTO6
if [ $? -ne 0 ]; then
	call_nmcli c modify $uuid ipv6.method link-local ||
		exit $ret
fi

[ $PROTO6 == "auto" ] && nmcli_clean_ip_and_gw $uuid 6

call_nmcli c up $uuid || exit $?
# end of script
