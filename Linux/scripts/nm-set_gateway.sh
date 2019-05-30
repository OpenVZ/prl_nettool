#!/bin/bash
# Copyright (c) 1999-2017, Parallels International GmbH
# Copyright (c) 2017-2019 Virtuozzo International GmbH. All rights reserved.
#
# This script configure gateway inside RedHat like VM.
#
# Parameters: <dev> <IP>
#   <dev>         - name of device. (example: eth2)
#   <IP>          - IP address of gateway
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
ETH_GATEWAY=$2
ETH_MAC=$3

uuid=`nm_check_and_create $ETH_DEV $ETH_MAC` ||
	exit $?

function set_gateway()
{
	local errors=0

	for gw in ${ETH_GATEWAY}; do
		if [ "${gw}" == "remove" ]; then
			call_nmcli c modify $uuid ipv4.gateway ""
		elif [ "${gw}" == "removev6" ]; then
			call_nmcli c modify $uuid ipv6.gateway ""
		elif is_ipv6 ${gw}; then
			call_nmcli c modify $uuid ipv6.gateway ${gw}
		else
			call_nmcli c modify $uuid ipv4.gateway ${gw}
		fi

		[ $? -ne 0 ] && errors=$((errors + 1))
	done

	call_nmcli c up $uuid || return $?

	return $errors
}

set_gateway
# end of script
