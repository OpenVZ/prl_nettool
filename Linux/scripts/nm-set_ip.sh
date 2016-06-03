#!/bin/bash
# Copyright (C) Parallels, 1999-2009. All rights reserved.
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

IP4_COUNT=0
IP6_COUNT=0

set_options "${OPTIONS}"

uuid=`nm_check_and_create $ETH_DEV $ETH_MAC` ||
	exit $?

function mask_section2digit()
{
	dec=$1
	digit=0

	while [ $dec -ne 0 ]; do
		first=$((dec & 128))
		if [ $first -ne 128 ]; then
			echo -1
			return 1
		fi

		dec=$(((dec << 1) & 255))
		digit=$((digit + 1))
	done

	echo $digit
}

function mask2cidr()
{
	mask=$1
	cidr=0
	count=0
	end=0
	
	IFS="." read -ra sections <<< "$mask"
	for section in "${sections[@]}"; do
		digit=`mask_section2digit $section`

		if [ $end -ne 0 -a "$digit" -ne 0 ]; then
			echo -1
			return 1
		elif [ "$digit" -lt 0 ]; then
			echo -1
			return 1
		elif [ "$digit" -ne 8 ]; then
			end=1
		fi

		cidr=$((cidr + digit))
		count=$((count + 1))
	done

	[ $count -ne 4 ] && echo -1

	echo $cidr
}

function set_ip()
{
	local ip_mask ip mask
	local new_ips
	local plusv4=""
	local plusv6=""
	local errors=0

	new_ips="${IP_MASKS}"
	for ip_mask in ${new_ips}; do
		if is_ipv6 ${ip_mask}; then
			let IP6_COUNT=IP6_COUNT+1
		else
			let IP4_COUNT=IP4_COUNT+1
		fi
	done

	call_nmcli c modify $uuid ipv4.method auto ||
		return $?
	call_nmcli c modify $uuid ipv6.method auto ||
		return $?

	new_ips="${IP_MASKS}"
	for ip_mask in ${new_ips}; do
		if ! is_ipv6 ${ip_mask}; then
			if echo ${ip_mask} | grep -q '/' ; then
				mask=`mask2cidr ${ip_mask##*/}`
			else
				mask=32
			fi
			ip=${ip_mask%%/*}

			if [ "$mask" -ge 0 ]; then
				call_nmcli c modify $uuid ${plusv4}ipv4.address $ip/$mask
				[ $? -ne 0 ] && errors=$((errors + 1))
				plusv4="+"
			fi
		else
			call_nmcli c modify $uuid ${plusv6}ipv6.address $ip_mask
			[ $? -ne 0 ] && errors=$((errors + 1))
			plusv6="+"
		fi
	done

	if [ $IP4_COUNT -eq 0 ]; then
		nmcli_clean_ip_and_gw $uuid 4
		if [ $USE_DHCPV4 -eq 0 ]; then
			call_nmcli c modify $uuid ipv4.method link-local ||
				return $?
		fi
	else
		call_nmcli c modify $uuid ipv4.method manual ||
			return $?
	fi

	if [ $IP6_COUNT -eq 0 ]; then
		nmcli_clean_ip_and_gw $uuid 6
		if [ $USE_DHCPV6 -eq 0 ]; then
			call_nmcli c modify $uuid ipv6.method link-local ||
				return $?
		fi
	else
		call_nmcli c modify $uuid ipv6.method manual ||
			return $?
	fi

	call_nmcli c up $uuid || return $?

	return $errors
}

set_ip
# end of script
