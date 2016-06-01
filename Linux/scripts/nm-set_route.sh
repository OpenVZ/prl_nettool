#!/bin/bash
# Copyright (C) Parallels, 1999-2011. All rights reserved.
#
# This script configure routes inside RedHat like VM.
#
# Parameters: <dev> <IP> <HWADDR>
#   <dev>         - name of device. (example: eth2)
#   <IP>          - IP address of gateway
#   <HWADDR>      - MAC address (not used)

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

function set_routes()
{
	call_nmcli c modify $uuid ipv4.routes ""
	call_nmcli c modify $uuid ipv6.routes ""

	local errors=0 args

	if [ "${ETH_GATEWAY}" != "remove" ] ; then
		for gw in ${ETH_GATEWAY}; do
			args=$(split_route $gw)
			if is_ipv6 ${gw}; then
				call_nmcli c modify $uuid +ipv6.routes "${args}"
			else
				call_nmcli c modify $uuid +ipv4.routes "${args}"
			fi

			[ $? -ne 0 ] && errors=$((errors + 1))
		done
	fi

	call_nmcli c up $uuid || return $?

	return $errors
}

set_routes
# end of script
