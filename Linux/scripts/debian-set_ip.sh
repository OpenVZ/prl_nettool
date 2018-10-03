#!/bin/bash
# Copyright (c) 2015-2017, Parallels International GmbH
#
# This script configure IP alias(es) inside Debian like VM.
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
	echo "File ${func} not found"
	exit 1
fi

ETH_DEV=$1
ETH_MAC=$2
IP_MASKS=$3
OPTIONS=$4
ETH_MAC_NW=`echo $ETH_MAC | sed "s,00,0,g"`
IFNUM=-1
IFNUM6=-1
IP4_MASKS=
IP6_MASK=
SET_AUTO[0]="no"

set_options "${OPTIONS}"

for ip_mask in ${IP_MASKS}; do
	if [ "${ip_mask}" == "remove" ] ; then
		continue
	elif is_ipv6 ${ip_mask}; then
		IP6_MASKS="${IP6_MASKS} ${ip_mask}"
	else
		IP4_MASKS="${IP4_MASKS} ${ip_mask}"
	fi
done


function print_ipv6_header()
{
	local device=$1
	local method=$2
	if [ "${SET_AUTO[0]}" != "yes" ] ; then
		SET_AUTO[0]="yes"
		echo "auto ${device}" >> $DEBIAN_CONFIGFILE
	fi

	echo "iface ${device} inet6 ${method}" >> $DEBIAN_CONFIGFILE

	# 2.6.35 kernel doesn't flush IPv6 addresses
	echo "	pre-down ip -6 addr flush dev ${device} scope global || :" >> $DEBIAN_CONFIGFILE
}

function add_ip6()
{
	local ip=$1
	local mask=$2
	local device=$3
	local ifnum=${IFNUM6}
	local ifnum_postfix=":${IFNUM6}"
	local inet="inet6"

	[ -z "${ip}" ] && \
		error "Empty value of IP"

	[ -z "${mask}" ] && \
		error "Empty value of MASK"


	if [ "x${IFNUM6}" == "x0" ] ; then
		print_ipv6_header ${device} static

		echo "	address ${ip}" >> $DEBIAN_CONFIGFILE
		echo "	netmask ${mask}" >> $DEBIAN_CONFIGFILE
	else
		awk 'BEGIN {found = 0}
		NF == 0 {next}
		!found && $1 == "iface" && $2 ~/'${device}'$/ && $3 == "inet6" {
			found = 1;
			print;
			next;
		}
		found == 1 && !/^\t/{
			print "\tup ip addr add '${ip}'/'${mask}' dev '${device}'";
			found++;
		}
		{print}
		END {
			if (found == 1) {
				print "\tup ip addr add '${ip}'/'${mask}' dev '${device}'";
			}
		}
		' < ${DEBIAN_CONFIGFILE} > ${DEBIAN_CONFIGFILE}.$$ && \
			mv -f ${DEBIAN_CONFIGFILE}.$$ ${DEBIAN_CONFIGFILE}
	fi
	
	echo >> $DEBIAN_CONFIGFILE
	echo >> $DEBIAN_CONFIGFILE
}

function create_config()
{
	local ip=$1
	local mask=$2
	local device=$3
	local ifnum=${IFNUM}
	local ifnum_postfix=":${IFNUM}"
	local inet="inet"

	[ -z "${ip}" ] && \
		error "Empty value of IP"

	[ -z "${mask}" ] && \
		error "Empty value of MASK"

	[ "x${IFNUM}" == "x0" ] && ifnum_postfix=""

	if [ "${SET_AUTO[${ifnum}]}" != "yes" ] ; then
		SET_AUTO[${ifnum}]="yes"
		echo "auto ${device}${ifnum_postfix}" >> $DEBIAN_CONFIGFILE
	fi

	if [ "${ip}" == "remove" ] ; then
		echo "" >> $DEBIAN_CONFIGFILE
		return
	fi

	echo "iface ${device}${ifnum_postfix} ${inet} static" >> $DEBIAN_CONFIGFILE

	echo "	address ${ip}
	netmask ${mask}" >> $DEBIAN_CONFIGFILE
	echo "	broadcast +
" >> $DEBIAN_CONFIGFILE
}

function set_ip()
{
	local ip_mask ip mask
	local new_ips

	remove_debian_interfaces ${ETH_DEV}
	remove_debian_interfaces "${ETH_DEV}:[0-9]+"

	new_ips="${IP_MASKS}"
	for ip_mask in ${new_ips}; do
		if is_ipv6 ${ip_mask} ; then
			let IFNUM6=IFNUM6+1
			if echo ${ip_mask} | grep -q '/' ; then
				mask=${ip_mask##*/}
			else
				mask='64'
			fi
			ip=${ip_mask%%/*}
			add_ip6 "${ip}" "${mask}" ${ETH_DEV}
		else
			let IFNUM=IFNUM+1
			if echo ${ip_mask} | grep -q '/' ; then
				mask=${ip_mask##*/}
			else
				mask='255.255.255.255'
			fi
			ip=${ip_mask%%/*}
			create_config "${ip}" "${mask}" ${ETH_DEV}
		fi
	done

	#clean IPv4
	ip -4 addr flush dev ${ETH_DEV}
	if [ "x$IP4_MASKS" == "x" ] ; then
		if [ $USE_DHCPV4 -eq 1 ] ; then
			echo "
iface ${ETH_DEV} inet dhcp
" >> $DEBIAN_CONFIGFILE
		fi
	fi

	# unset IPv6 addresses on interface down
	[ "x$IP6_MASKS" == "x" ] && print_ipv6_header ${ETH_DEV} manual

	if [ "x$IP6_MASKS" == "x" -a $USE_DHCPV6 -eq 1 ] ; then
		#don't support dhcpv6 by config
		set_wide_dhcpv6 ${ETH_DEV}
	else
		unset_wide_dhcpv6 ${ETH_DEV}
	fi
}

if is_netplan_controlled; then
	$path/$NETPLAN_CFG -a "set_ip" -d "$ETH_DEV" -i "$IP_MASKS" -o "$OPTIONS"
	exit $?
elif [ -f $NWSYSTEMCONF -o -f $NMCONFFILE ]; then
	call_nm_script $0 "$@"
	exit $?
else
	set_ip
fi

$path/debian-restart.sh ${ETH_DEV}

exit 0
# end of script
