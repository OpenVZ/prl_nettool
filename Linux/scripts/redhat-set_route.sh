#!/bin/bash
# Copyright (c) 2015 Parallels IP Holdings GmbH
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

ETH_DEV_CFG=route-$ETH_DEV

IFCFG_DIR=/etc/sysconfig/network-scripts
IFCFG=${IFCFG_DIR}/${ETH_DEV_CFG}

function set_routes()
{
	local is_changed="no"

	if [ -f ${IFCFG} ] ; then
		/bin/mv -f ${IFCFG} ${IFCFG}.bak 
		is_changed="yes"
	fi

	if [ "${ETH_GATEWAY}" != "remove" ] ; then
		for gw in ${ETH_GATEWAY}; do
			echo "${gw} dev ${ETH_DEV} scope link" >> $IFCFG
			is_changed="yes"
		done
	fi
	
	if [ "$is_changed" == "yes" ] ; then
		is_device_up ${ETH_DEV} && /sbin/ifdown ${ETH_DEV}
		/sbin/ifup ${ETH_DEV}
	fi
}

is_nm_active
if [ $? -eq 0 ]; then
	call_nm_script $0 "$@"
	exit $?
fi

set_routes

# end of script
