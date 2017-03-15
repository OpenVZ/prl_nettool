#!/bin/bash
# Copyright (c) 1999-2017, Parallels International GmbH
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

uuid=`nm_check_and_create $DEV $ADDR` ||
	exit 2

[ "x$PROTO" == "x" ] && PROTO=4

dhcp=`nm_get_if_field $DEV ipv$PROTO.method`
[ $? -ne 0 ] && exit 2

[ "$dhcp" == "auto" ] && exit 0

exit 1

