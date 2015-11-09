#!/bin/bash
# Copyright (c) 2015 Parallels IP Holdings GmbH
#
# This script restart network inside Debian like VM.
#
  

prog="$0"
path="${prog%/*}"
funcs="$path/functions"
CONFIGFILE="/etc/network/interfaces"

if [ -f "$funcs" ] ; then
	. $funcs
else
	echo "Program $prog"
	echo "File ${func} not found"
	exit 1
fi

function restart()
{
	if [ -x /usr/sbin/service ]; then
		# Ubuntu 12.04 contains only start in upstart config
		/etc/init.d/networking stop > /dev/null 2>&1 </dev/null
		/usr/sbin/service networking restart
		if [ $? -ne 0 ]; then
			/sbin/ifdown -a
			/sbin/ifup -a
		fi
	else
		# Restart may be deprecated
		/etc/init.d/networking stop </dev/null
		/etc/init.d/networking start </dev/null
	fi
}


function restart_nm() {
	# In ubuntu 14.04 Desktop or based distros like Linux Mint
	# network-manager is restarted and watched by initctl
	type initctl > /dev/null 2>&1 &&
		initctl restart network-manager
	if [ $? -ne 0 ]; then
		$NWMANAGER restart
	fi
}

if [ -f $NWSYSTEMCONF -o -f $NMCONFFILE ]; then
	restart_nm
else
	restart
fi

exit 0
# end of script
