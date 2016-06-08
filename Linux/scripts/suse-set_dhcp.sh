#!/bin/bash
# Copyright (c) 2015 Parallels IP Holdings GmbH
#
# This script configure adapter to use DHCP inside SuSE like VM.
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
IFCFG_DIR=/etc/sysconfig/network

PROTO4="no"
PROTO6="no"

for proto in ${PROTO}; do
	if [ "x$proto" == "x4" ] ; then
		PROTO4="yes"
	elif [ "x$proto" == "x6" ] ; then
		PROTO6="yes"
	fi
done


function create_config()
{
	local dhcp_type="dhcp"

	if [ "x$PROTO4" == "xyes" -a "x$PROTO6" != "xyes" ] ; then
		dhcp_type="dhcp4"
	elif [ "x$PROTO6" == "xyes" -a "x$PROTO4" != "xyes" ] ; then
		dhcp_type="dhcp6"
	fi

	echo "BOOTPROTO='${dhcp_type}'
BROADCAST=''
ETHTOOL_OPTIONS=''
MTU=''
NETWORK=''
USERCONTROL='no'
STARTMODE='auto'"  > ${IFCFG_DIR}/bak/${ETH_DEV_CFG} || \
	error "Unable to create interface config file" ${VZ_FS_NO_DISK_SPACE}
}


function backup_configs()
{
	local delall=$1

	rm -rf ${IFCFG_DIR}/bak/ >/dev/null 2>&1
	mkdir -p ${IFCFG_DIR}/bak
	[ -n "${delall}" ] && return 0

	cd ${IFCFG_DIR} || return 1
	${CP} ${ETH_DEV_CFG} ${IFCFG_DIR}/bak/
}

function move_configs()
{
	cd ${IFCFG_DIR} || return 1
	mv -f bak/* ${IFCFG_DIR}/ >/dev/null 2>&1 
	rm -rf ${IFCFG_DIR}/bak
}

function set_dhcp()
{
	backup_configs
	create_config
	move_configs

	is_device_up ${ETH_DEV} && /sbin/ifdown ${ETH_DEV}

	/sbin/ifup ${ETH_DEV}
}

get_suse_config_name $ETH_DEV $ETH_MAC
set_dhcp

exit 0
# end of script
