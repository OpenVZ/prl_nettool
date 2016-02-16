#!/bin/bash
# Copyright (c) 2015 Parallels IP Holdings GmbH
#
# This script detect is DHCP enabled for RedHat like VMs.
#
# Arguments: <ADDR> <DEV> <PROTO>
#   ADDR       - hardware address of adapter
#   DEV        - name of device ( eth1 )
#   PROTO      - proto "4" or "6". If empty - 4.
#
# Return:
#   2 - can't detect or some error
#   0 - enabled
#   1 - disabled 
#

prog="$0"
path="${prog%/*}"
funcs="$path/functions"

if [ -f "$funcs" ] ; then
	. $funcs
else
	echo "Program $0"
	echo "'$funcs' was not found"
	exit 2
fi

ADDR=$1
DEV=$2
PROTO=$3


nmscript=$path/nm-get_dhcp.sh

is_nm_active
if [ $? -eq 0 -a -f $nmscript ]; then
	$nmscript "$@"
	exit $?
fi

__sed_discard_ignored_files='/\(~\|\.bak\|\.orig\|\.rpmnew\|\.rpmorig\|\.rpmsave\|\:[0-9]*\)$/d'

function get_config_by_hwaddr ()
{
    LANG=C egrep -il "^[[:space:]]*HWADDR=.*${1}.*" /etc/sysconfig/network-scripts/ifcfg-* \
	| LC_ALL=C sed -e "$__sed_discard_ignored_files"
}

function get_config_by_dev_name ()
{
	LANG=C grep -l "^[[:space:]]*DEVICE=[\"]*${1}[\"]*\([[:space:]#]\|$\)" /etc/sysconfig/network-scripts/ifcfg-* \
		| LC_ALL=C sed -e "$__sed_discard_ignored_files"
}

CONFIG=`get_config_by_hwaddr ${ADDR}`

if [ ! -f "${CONFIG}" ] ; then
	CONFIG=`get_config_by_dev_name ${DEV}`
fi

#config was not found
[ -f "${CONFIG}" ] || exit 2

if [ "x$PROTO" != "x6" ] ; then
	LANG=C egrep -iq "^[[:space:]]*BOOTPROTO=.*?dhcp.*?" "${CONFIG}"
else
	LANG=C egrep -iq "^[[:space:]]*DHCPV6C=.*?yes.*?" "${CONFIG}"
fi

if [ $? -eq 0 ] ; then
    exit 0
fi

exit 1

