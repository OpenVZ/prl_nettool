#!/bin/bash
# Copyright (c) 2020 Virtuozzo International GmbH. All rights reserved.
#
# This script restart network inside Debian-based VM controlled by netplan.
#

prog="$0"
path="${prog%/*}"
funcs="$path/functions"

if [ -f "$funcs" ] ; then
	. $funcs
else
	echo "Program $prog"
	echo "File ${funcs} not found"
	exit 1
fi

if is_netplan_controlled; then
	$path/$NETPLAN_CFG -a "restart" -d "$1"
	exit $?
else
	exit 0
fi

# end of script
