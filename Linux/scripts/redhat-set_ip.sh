#!/bin/bash
# Copyright (c) 2015-2017, Parallels International GmbH
# Copyright (c) 2017-2019 Virtuozzo International GmbH. All rights reserved.
#
# This script configure IP alias(es) inside RedHat like VM.
#
# Parameters: <dev> <MAC_addr> <IPs>
#   <dev>         - name of device. (example: eth2)
#   <MAC_addr>    - MAC address of device
#   <IP/MASKs>    - IP address(es) with MASK 
#                   (example: 192.169.1.30/255.255.255.0)
#                   (several addresses should be divided by space)
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
IP_MASKS=$3
OPTIONS=$4
ETH_DEV_CFG=ifcfg-$ETH_DEV
IFNUM=-1
IF6NUM=-1

IP4_COUNT=0
IP6_COUNT=0

IFCFG_DIR=/etc/sysconfig/network-scripts
IFCFG=${IFCFG_DIR}/${ETH_DEV_CFG}
NETFILE=/etc/sysconfig/network
ROUTE=${IFCFG_DIR}/route-${ETH_DEV}
RESTART_NETWORK="no"

set_options "${OPTIONS}"
disable_network_manager

function setup_network()
{
	mkdir -p ${IFCFG_DIR}

	is_nm_present || return

	# Set /etc/sysconfig/network
	if ! grep -ilq "^[[:space:]]*NETWORKING=\"*yes" $NETFILE 2>/dev/null ; then
		put_param $NETFILE NETWORKING yes
		RESTART_NETWORK="yes"
	fi
}

function create_config()
{
	local ip=$1
	local mask=$2
	local ifnum=$3
	local ifnum_postfix=":${ifnum}"
	# Use the new scheme only for Fedoras with systemd and Network Manager installed
	is_nm_present
	local is_nm_controlled=$?

	[ -z "${ip}" ] && \
		error "Empty value of IP"

	[ "${ip}" == "remove" ] && ip=""

	[ -z "${mask}" ] && \
		error "Empty value of MASK"

	[ "x${ifnum}" == "x0" ] && ifnum_postfix=""

	[ ${is_nm_controlled} -eq 1 ] && ifnum_postfix=""

	local ifcfg=${IFCFG_DIR}/bak/${ETH_DEV_CFG}${ifnum_postfix}
	touch ${ifcfg}

	if [ "x${ifnum}" == "x0" -o ${is_nm_controlled} -eq 0 ]; then
		put_param ${ifcfg} DEVICE "${ETH_DEV}${ifnum_postfix}"
		put_param ${ifcfg} ONBOOT yes
		put_param ${ifcfg} BOOTPROTO none
		put_param ${ifcfg} HWADDR ${ETH_MAC}
		if [ -n "${ifnum_postfix}" ]; then
			put_param ${ifcfg} NO_ALIASROUTING yes
		fi
	fi
	if [ ${is_nm_controlled} -eq 1 ]; then
		put_param ${ifcfg} IPADDR${ifnum} "${ip}"
		put_param ${ifcfg} NETMASK${ifnum} ${mask}
	else
		put_param ${ifcfg} IPADDR "${ip}"
		put_param ${ifcfg} NETMASK ${mask}
	fi
	
	if [ "x${ifnum}" == "x0" ] ; then
		if [ $IP6_COUNT -eq 0 -a $USE_DHCPV6 -eq 1 ]; then
			put_param ${ifcfg} DHCPV6C yes
			put_param ${ifcfg} DHCPV6C_OPTIONS "-d"
		fi
	fi
}

function add_ip6()
{
        local ifcfg="${IFCFG_DIR}/bak/${ETH_DEV_CFG}"
        local ip=$1
        local mask=$2
        local ipm
	local ifnum=$3

	touch ${ifcfg}

	if [ $ifnum -eq 0 ] ; then
		del_param "${ifcfg}" IPV6ADDR_SECONDARIES
		del_param "${ifcfg}" IPV6ADDR
		put_param ${ifcfg} DHCPV6C no
		put_param ${ifcfg} IPV6_AUTOCONF no
		
		if [ $IP4_COUNT -eq 0 ]; then
			put_param ${ifcfg} DEVICE "${ETH_DEV}${ifnum_postfix}"
			put_param ${ifcfg} ONBOOT yes
			if [ $USE_DHCPV4 -eq 1 ]; then
				put_param ${ifcfg} BOOTPROTO dhcp
			else
				put_param ${ifcfg} BOOTPROTO none
			fi
			put_param ${ifcfg} HWADDR ${ETH_MAC}
		fi
	fi

        put_param ${NETFILE} NETWORKING_IPV6 yes
        put_param ${ifcfg} DEVICE "${ETH_DEV}" 
        put_param ${ifcfg} IPV6INIT yes
        if ! grep -qw "${ip}" ${ifcfg} 2>/dev/null; then
                if [ -n "${mask}" ]; then
                        ipm="${ip}/${mask}"
                else
                        ipm="${ip}"
                fi

		if [ $ifnum -eq 0 ] ; then
			put_param ${ifcfg} IPV6ADDR "${ipm}"
		else
			add_param ${ifcfg} IPV6ADDR_SECONDARIES "${ipm}"
		fi
        fi
}

function move_configs()
{
	pushd ${IFCFG_DIR} > /dev/null 2>&1 || return 1
	rm -rf ${ETH_DEV_CFG}*
	mv -f bak/* ${IFCFG_DIR}/ >/dev/null 2>&1 
	rm -rf ${IFCFG_DIR}/bak
	popd > /dev/null 2>&1
}

function remove_ips()
{
        local ifcfg="${IFCFG_DIR}/bak/${ETH_DEV_CFG}"
        local old_ifcfg="${IFCFG_DIR}/${ETH_DEV_CFG}"

		[ ! -f $old_ifcfg ] && return 0

		cat $old_ifcfg > $ifcfg || error "Cant copy ${ETH_DEV_CFG}" $VZ_FS_NO_DISK_SPACE

		del_param "${ifcfg}" IPV6ADDR_SECONDARIES
		del_param "${ifcfg}" IPV6ADDR
		del_param "${ifcfg}" "IPADDR\d*"
		del_param "${ifcfg}" "NETMASK\d*"
		return 0
}

function set_ip()
{
	local ip_mask ip mask
	local new_ips

	rm -rf ${IFCFG_DIR}/bak/ >/dev/null 2>&1
	mkdir -p ${IFCFG_DIR}/bak

	new_ips="${IP_MASKS}"
	for ip_mask in ${new_ips}; do
		[ "${ip_mask}" = "remove" ] && remove_ips && return $?
		if is_ipv6 ${ip_mask}; then
			let IP6_COUNT=IP6_COUNT+1
		else
			let IP4_COUNT=IP4_COUNT+1
		fi
	done

	setup_network

	new_ips="${IP_MASKS}"
	for ip_mask in ${new_ips}; do
		if ! is_ipv6 ${ip_mask}; then
			let IFNUM=IFNUM+1
			if echo ${ip_mask} | grep -q '/' ; then
				mask=${ip_mask##*/}
			else
				mask='255.255.255.255'
			fi
			ip=${ip_mask%%/*}
			create_config "${ip}" "${mask}" "${IFNUM}"
		else
			let IF6NUM=IF6NUM+1
			if echo ${ip_mask} | grep -q '/' ; then
				mask=${ip_mask##*/}
			else
				mask=
			fi
			ip=${ip_mask%%/*}
			add_ip6 "${ip}" "${mask}" "${IF6NUM}"
		fi
	done
}

function apply()
{
	#stop adapter
	if [ "x$RESTART_NETWORK" = "xyes" ]; then
		/etc/init.d/network stop
	else
		/sbin/ifdown ${ETH_DEV}
	fi

	move_configs

	#start adapter
	if [ "x$RESTART_NETWORK" = "xyes"  ]; then
		/etc/init.d/network start
	else
		/sbin/ifup ${ETH_DEV}
	fi
}

is_nm_active
if [ $? -eq 0 ]; then
	call_nm_script $0 "$@"
	exit $?
fi

set_ip && apply

# end of script
