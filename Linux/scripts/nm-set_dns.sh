#!/bin/bash
# Copyright (c) 2018 Virtuozzo International GmbH.  All rights reserved.
#
# This script sets up resolver inside VM controlled with NM
#
# arguments: <NAMESERVER> <SEARCHDOMAIN> <HOSTNAME>
#   <SEARCHDOMAIN>
#       Sets search domain(s).
#   <NAMESERVER>
#       Sets name server(s).

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

NAMESERVER="$3"
SEARCHDOMAIN="$4"
HOSTNAME="$5"

if [ -z "$ETH_DEV" ]; then
	uuid=`LC_ALL=C nmcli -t c show --active | cut -d: -f2` ||
		exit $?
else
	uuid=`nm_check_and_create $ETH_DEV $ETH_MAC` ||
		exit $?
fi

[ `expr length "$uuid"` -eq 36 ] ||
	exit $?

function set_dns()
{
	local errors=0
	local nameservers="$1"

	# nothing to do
	[ -z "${nameservers}" ] && return 0

	local ns4=""
	local ns6=""
	for ip in ${nameservers}; do
		if is_ipv6 ${ip}; then
			[ -z "${ns6}" ] || ns6="${ns6},"
			ns6="${ns6}${ip}"
		else
			[ -z "${ns4}" ] || ns4="${ns4},"
			ns4="${ns4}${ip}"
		fi
	done

	call_nmcli c modify $uuid ipv4.dns "${ns4}"
	call_nmcli c modify $uuid ipv6.dns "${ns6}"

	return $errors
}

function set_domains()
{
	local errors=0
	local domains="$1"

	# nothing to do
	[ -z "${domains}" ] && return 0

	domains=`echo "${domains}" | tr -s ' ' ','`

	call_nmcli c modify $uuid ipv4.dns-search "${domains}"

	return $errors
}
function set_hostname()
{
	local errors=0
	local hostname="$1"

	# nothing to do
	[ -z "${hostname}" ] && return 0

	call_nmcli general hostname "${hostname}"

	return $errors
}

set_dns "${NAMESERVER}"
set_domains "${SEARCHDOMAIN}"
set_hostname "${HOSTNAME}"

call_nmcli c up ${uuid}

exit 0
