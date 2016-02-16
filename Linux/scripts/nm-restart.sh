#!/bin/bash
# Copyright (C) Parallels, 1999-2009. All rights reserved.
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
	exit 1
fi
  
service_command NetworkManager stop &&
service_command network restart &&
service_command NetworkManager start
# end of script
