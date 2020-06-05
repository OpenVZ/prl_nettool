#!/bin/bash
# Copyright (c) 2015-2017, Parallels International GmbH
# Copyright (c) 2017-2019 Virtuozzo International GmbH. All rights reserved.
#
# This script configure gateway inside Debian like VM.
#
# Parameters: <dev> <IP> <MAC_addr>
#   <dev>         - name of device. (example: eth2)
#   <IP>          - IP address of gateway
#   <MAC_addr>    - MAC address of device
#

prog="$0"
path="${prog%/*}"
funcs="$path/functions"


if [ -f "$funcs" ]; then
	. $funcs
else
	echo "Program $0"
	echo "File $funcs not found"
	exit 1
fi

ETH_DEV=$1
ETH_GATEWAY=$2
ETH_MAC=$3
ETH_MAC_NW=`echo $ETH_MAC | sed "s,00,0,g"`

if is_netplan_controlled; then
	$path/$NETPLAN_CFG -a "set_gateway" -d "$ETH_DEV" -i "$ETH_GATEWAY"
	exit $?
elif [ -f $NWSYSTEMCONF -o -f $NMCONFFILE ]; then
	call_nm_script $0 "$@"
	exit $?
else
	for gw in ${ETH_GATEWAY}; do
		inet="inet"

		if [ "${gw}" == "remove" -o "${gw}" == "removev6" ] ; then
			continue
		fi

		if is_ipv6 ${gw}; then
			inet="inet6"
		fi

		if which route >/dev/null 2>&1
		then
			route="route -A"
			add="add"
			via="gw"
		else
			route="ip -f"
			add="route add"
			via="via"
		fi
		
		awk "
			/^\\tup ${route} ${inet} ${add} .* dev ${ETH_DEV}/ { next; }
			/^\\tup ${route} ${inet} ${add} default/ { next; }
			/^\\taddress/ { print; next; }
			/^\\tnetmask/ { print; next; } 
			/^\\tpre-down/ { print; next; } 
			/^\\tup ip/ { print; next; } 
			/^\\tbroadcast/ { print; next; }
			\$1 == \"iface\" && \$2 ~/${ETH_DEV}\$/ && \$3 == \"${inet}\" { addgw=1; print; next; }
			addgw { 
				print \"\\tup ${route} ${inet} ${add} ${gw} dev ${ETH_DEV}\";
				print \"\\tup ${route} ${inet} ${add} default ${via} ${gw} dev ${ETH_DEV}\";
				addgw=0
			}
			{ print }
		" < ${DEBIAN_CONFIGFILE} > ${DEBIAN_CONFIGFILE}.$$ && \
			mv -f ${DEBIAN_CONFIGFILE}.$$ ${DEBIAN_CONFIGFILE}
	done

fi

exit 0
# end of script
