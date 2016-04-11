#!/bin/bash
# Copyright (c) 2015 Parallels IP Holdings GmbH
#
# This script configure adapter to use DHCP inside RedHat like VM.
#
# Parameters: <dev> <mac>
#   <dev>         - name of device. (example: eth2)
#   <mac>         - hardware address of device
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
PROTO=$3
ETH_DEV_CFG=ifcfg-$ETH_DEV

IFCFG_DIR=/etc/sysconfig/network-scripts
IFCFG=${IFCFG_DIR}/${ETH_DEV_CFG}
NETFILE=/etc/sysconfig/network
HOSTFILE=/etc/hosts

disable_network_manager

PROTO4="none"
PROTO6="no"

for proto in ${PROTO}; do
	if [ "x$proto" == "x4" ] ; then
		PROTO4="dhcp"
	elif [ "x$proto" == "x6" ] ; then
		PROTO6="yes"
	fi
done

function create_config()
{
	echo "DEVICE=${ETH_DEV}
ONBOOT=yes
BOOTPROTO=${PROTO4}
DHCPV6C=${PROTO6}
DHCPV6C_OPTIONS=-d
HWADDR=${ETH_MAC}
TYPE=Ethernet" > ${IFCFG_DIR}/bak/${ETH_DEV_CFG} || \
	error "Unable to create interface config file" ${VZ_FS_NO_DISK_SPACE}
}



function backup_configs()
{
	local delall=$1

	rm -rf ${IFCFG_DIR}/bak/ >/dev/null 2>&1
	mkdir -p ${IFCFG_DIR}/bak
	[ -n "${delall}" ] && return 0

	pushd ${IFCFG_DIR} > /dev/null 2>&1 || return 1
	if ls ${ETH_DEV_CFG}:* > /dev/null 2>&1; then
		${CP} ${ETH_DEV_CFG}:* ${IFCFG_DIR}/bak/ || \
			error "Unable to backup intrface config files" ${VZ_FS_NO_DISK_SPACE}
	fi
	popd > /dev/null 2>&1
}

function move_configs()
{
	pushd ${IFCFG_DIR} > /dev/null 2>&1 || return 1
	rm -rf ${ETH_DEV_CFG}:*
	mv -f bak/* ${IFCFG_DIR}/ >/dev/null 2>&1 
	rm -rf ${IFCFG_DIR}/bak
	popd > /dev/null 2>&1
}

function set_dhcp()
{
	local delall=`[ "${PROTO4}" == "dhcp" ] && echo -n "yes"`
	backup_configs $delall
	create_config
	move_configs

	is_device_up ${ETH_DEV} && /sbin/ifdown ${ETH_DEV}
	/sbin/ifup ${ETH_DEV}
}

is_nm_active
if [ $? -eq 0 ]; then
	call_nm_script $0 "$@"
	exit $?
fi

set_dhcp

# end of script
