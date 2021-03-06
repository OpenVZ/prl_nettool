#!/bin/bash
# Copyright (c) 2015-2017, Parallels International GmbH
# Copyright (c) 2017-2020 Virtuozzo International GmbH. All rights reserved.
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
	$path/$NETPLAN_CFG -a "set_route" -d "$ETH_DEV" -i "$ETH_GATEWAY"
	exit $?
elif [ -f $NWSYSTEMCONF -o -f $NMCONFFILE ]; then
	call_nm_script $0 "$@"
	exit $?
else
	for gw in ${ETH_GATEWAY}; do
		inet="inet"

		copy_fields=("address" "netmask" "broadcast")
		if is_ipv6 ${gw} || [ "${gw}" = "removev6" ] ; then
			inet="inet6"
			copy_fields=("${copy_fields[@]}" "pre-down" "up ip")
		else
			copy_fields=("${copy_fields[@]}" "pre-up")
		fi

		cmd=
		for f in "${copy_fields[@]}"; do
			cmd="$cmd"'
				/^\t'"$f"'/ {print; next; }'
		done


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

		if [ "${gw}" == "remove" -o "${gw}" == "removev6" ] ; then
			cmd="$cmd"'
				/^\tup '${route}' '${inet}' '${add}' .* dev '${ETH_DEV}'/ { next; }'
		else
			ip=$(route_ip $gw)
			hop=$(route_gw $gw)
			[ -n "$hop" ] && hop="$via $hop"
			metric=$(route_metric $gw)
			[ -n "$metric" ] && metric="metric $metric"
			cmd="$cmd"'
				$1 == "iface" && $2 ~/'${ETH_DEV}'$/ && $3 == "'${inet}'" { addgw=1; print; next; }
				addgw {
					print "\tup '${route}' '${inet}' '${add}' '${ip}' '"${hop}"' dev '${ETH_DEV}' '"${metric}"'";
					addgw=0
				}'
		fi
		cmd="$cmd"'
			{ print }'

		awk "${cmd}" < ${DEBIAN_CONFIGFILE} > ${DEBIAN_CONFIGFILE}.$$ && \
			mv -f ${DEBIAN_CONFIGFILE}.$$ ${DEBIAN_CONFIGFILE}
	done
fi

$path/debian-restart.sh ${ETH_DEV}

exit 0
# end of script
