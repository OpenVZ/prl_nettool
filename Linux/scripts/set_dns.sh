#!/bin/bash
# Copyright (c) 2015 Parallels IP Holdings GmbH
#
# This script sets up resolver inside VM
#
# arguments: <NAMESERVER> <SEARCHDOMAIN>
#   <SEARCHDOMAIN>
#       Sets search domain(s). Modifies /etc/resolv.conf
#   <NAMESERVER>
#       Sets name server(s). Modifies /etc/resolv.conf

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
DISTR="$6"

RESOLVDIR=/etc/resolvconf
RESOLVCONF_LNK="${RESOLVDIR}/run/resolv.conf"
RESOLVCONF="${RESOLVDIR}/resolv.conf.d/base"
OSRELEASE=/etc/os-release


function get_dhclient_conf()
{
	local distr="$1"

	# Distro-specific defaults
	local cfgfile="/etc/dhclient.conf"

	# Ubuntu
	if [ "${distr}" = "debian" -a -e "${OSRELEASE}" -a \
		 "$(. ${OSRELEASE} 2>/dev/null && echo $ID)" = "ubuntu" ]; then
		cfgfile="/etc/dhcp3/dhclient.conf"
	# Normal debian uses /etc/dhcp/
	elif [ "${distr}" = "debian" ]; then
		cfgfile="/etc/dhcp/dhclient.conf"
	# Redhat will prefer /etc/dhcp/
	elif [ "${distr}" = "redhat" ]; then
		cfgfile="/etc/dhcp/dhclient.conf"
	# SUSE uses /etc/dhclient.conf by default
	elif [ "${distr}" = "suse" ]; then
		cfgfile="/etc/dhclient.conf"
	# Assume other distros prefer /etc/dhcp if exists
	elif [ -d "/etc/dhcp" ]; then
		cfgfile="/etc/dhcp/dhclient.conf"
	fi

	echo "${cfgfile}"
}

function set_dns()
{
	local cfgfile="$1"
	local server="$2"
	local search="$3"
	local dhclientconf="$(get_dhclient_conf $4)"
	local srv fname search_value server_value

	if [ -L ${cfgfile} ]; then
		# resolvconf configuration
		fname="$(readlink "${cfgfile}")"
		if [ "${fname}" = "${RESOLVCONF_LNK}" ]; then
			cfgfile=${RESOLVCONF}
		fi
	fi 

	[ -d "$(dirname ${dhclientconf})" ] || mkdir -p "$(dirname ${dhclientconf})"
	[ -f "${dhclientconf}" ] || touch "${dhclientconf}"

	if [ -n "${search}" ]; then
		# Multiple spaces in name are unsupported by put_param2
		sed -i "/prepend\s\+domain-search.*/d" ${dhclientconf} || \
				error "Can't change file ${dhclientconf}" ${VZ_FS_NO_DISK_SPACE}
		if [ "${search}" = '#' ]; then
			sed -i "/search.*/d" ${cfgfile} || \
				error "Can't change file ${cfgfile}" ${VZ_FS_NO_DISK_SPACE} 
		else
			put_param2 "${cfgfile}" search "${search}"
			for srch in ${search}; do
				[ -n "${search_value}" ] && search_value="${search_value}, "
				search_value="${search_value}\"${srch}\""
			done
			put_param2 "${dhclientconf}" "prepend domain-search" "${search_value};"
		fi
	fi
	if [ -n "${server}" ]; then
		[ -f ${cfgfile} ] || touch ${cfgfile}
		sed -i "/nameserver.*/d" ${cfgfile} || \
			error "Can't change file ${cfgfile}" ${VZ_FS_NO_DISK_SPACE} 
		# Multiple spaces in name are unsupported by put_param2
		sed -i "/prepend\s\+domain-name-servers.*/d" ${dhclientconf} || \
				error "Can't change file ${cfgfile}" ${VZ_FS_NO_DISK_SPACE}
		[ "${server}" = '#' ] && return
		for srv in ${server}; do
			echo "nameserver ${srv}" >> ${cfgfile} || \
				error "Can't change file ${cfgfile}" ${VZ_FS_NO_DISK_SPACE} 
			[ -n "${server_value}" ] && server_value="${server_value}, "
			server_value="${server_value}${srv}"
		done
		put_param2 "${dhclientconf}" "prepend domain-name-servers" "${server_value};"
	fi
	chmod 644 ${cfgfile}
	chmod 644 ${dhclientconf}
}

function set_hostname()
{
	local hostname="$1"
	local distr="$2"

	# nothing to do
	if [ -z "${hostname}" ]; then
		exit 0
	fi

	hostname "${hostname}";

	if [ "${distr}" = "redhat" ]; then
		put_param "/etc/sysconfig/network" "HOSTNAME" "${hostname}"	
	elif [ "${distr}" = "suse" ]; then
		echo "${hostname}" > /etc/HOSTNAME
	elif [ "${distr}" = "debian" ]; then
		echo "${hostname}" > /etc/hostname
	fi
}

is_nm_active
if [ $? -eq 0 ]; then
        call_nm_script $0 "$@"
        exit $?
fi

set_dns /etc/resolv.conf "${NAMESERVER}" "${SEARCHDOMAIN}" "${DISTR}"
set_hostname "${HOSTNAME}" "${DISTR}"

exit 0
