#!/bin/bash
# Copyright (c) 2015-2017, Parallels International GmbH
# Copyright (c) 2017-2019 Virtuozzo International GmbH. All rights reserved.
#
# This script configure default route inside SuSE like VM.
#
# Parameters: <dev> <IP>
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

IFCFG_DIR=/etc/sysconfig/network/
IFCFG=${IFCFG_DIR}/routes

function set_routes()
{
	local hop metric

	[ -f ${IFCFG} ] && \
		/bin/rm -f ${IFCFG} > /dev/null 2>&1
	touch ${IFCFG}

	for gw in ${ETH_GATEWAY}; do
		if [ "${gw}" != "remove" -a "${gw}" != "removev6" ] ; then
			hop=$(route_gw $gw)
			[ -z "$hop" ] && hop="-"
			metric=$(route_metric $gw)
			[ -n "$metric" ] && metric="metric $metric"
			echo "$(route_ip $gw) $hop - ${ETH_DEV} $metric" >> ${IFCFG}
		fi
	done
	
	is_device_up ${ETH_DEV} && /sbin/ifdown ${ETH_DEV}

	/sbin/ifup ${ETH_DEV}
}

set_routes

exit 0
# end of script
