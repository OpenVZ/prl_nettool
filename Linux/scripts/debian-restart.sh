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

function restart_conf()
{
	/sbin/ifdown $1 &&
		/sbin/ifup $1
}

function restart_iface()
{
	local iface=$1

	restart_conf $iface || return $?
	
	conf_suffix=(`cat /etc/network/interfaces | grep "^\s*auto $iface:.*$" | sed "s@\s*auto $iface:\(\d*\)\s*@\1@"`)
	for suffix in ${conf_suffix[@]}
	do
		restart_conf "$iface:$suffix" || return $?
	done
}

function restart()
{
	if [ $# -ne 0 ]; then
		restart_iface $1 &&
			return 0
	fi

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
	type nmctl > /dev/null 2>&1
	if [ $? -eq 0 -a $# -ne 0 ]; then
		nmcli device disconnect $1 &&
			nmcli device connect $1 &&
			return 0
	fi

	# In ubuntu 14.04 Desktop or based distros like Linux Mint
	# network-manager is restarted and watched by initctl
	type initctl > /dev/null 2>&1 &&
		initctl restart network-manager
	if [ $? -ne 0 ]; then
		$NWMANAGER restart
	fi
}

if [ -f $NWSYSTEMCONF -o -f $NMCONFFILE ]; then
	restart_nm $1
else
	restart $1
fi

exit 0
# end of script
