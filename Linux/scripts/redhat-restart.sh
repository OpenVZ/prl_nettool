#!/bin/bash
# Copyright (c) 2015-2017, Parallels International GmbH
#
# This script restart network inside RedHat like VM.
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

call_nm_script $0 "$@" || /etc/init.d/network restart
# end of script
