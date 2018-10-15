#!/usr/bin/python3
#
# Copyright (c) 2018 Virtuozzo International GmbH.  All rights reserved.
#
# This python script provides interface to configure network settings
# using netplan.

import argparse
import yaml
import os
import re
import subprocess
import sys


NETPLAN_CFG_DIR = "/etc/netplan/"
NETPLAN_CFG_PREFIX = "90-vz-"


def getArgParser():
	"""
	Init argparse
		- "device" is mandatory for all commands.
		- "ip" is used for gateway \ route related operations
	"""
	parser = argparse.ArgumentParser("Netplan configuration editor")

	parser.add_argument('-a', '--action', action="store",
			choices=("get_dhcp", "restart", "set_dhcp", "set_gateway", "set_ip", "set_route"))  
	parser.add_argument('-i', '--ip', action="store")
	parser.add_argument('-d', '--device', action="store", required=True)
	parser.add_argument('-p', '--proto', action="store")
	parser.add_argument('-o', '--options', action="store", default="")

	return parser

def is_ip_proto(addr, proto):
	res = None
	if int(proto) == 4:
		res = "." in list(addr)
	elif int(proto) == 6:
		res = ":" in list(addr)

	if res:
		return True
	return False

def split_route(route):
	"""
	Split route string into elemenets
	 "X.X.X.X/Z=X.X.X.Ym100" -> "X.X.X.X/Z X.X.X.Y 100"
	Return tuple (to, via, metric)
	"""
	res = re.split("[m=]", route)
	to = res[0]
	via = res[1]
	if res.__len__() > 2:
		metric = res[2]
	else:
		metric = ""
	return to, via, metric

def netmask_to_cidr(netmask):
	"""
	Convert netmask to CIDR notation. Netplan does not accept netmasks
	"""
	return sum([bin(int(x)).count('1') for x in netmask.split('.')])

class npConfig(object):
	def __init__(self, **kwargs):
		self._ifname = kwargs["device"]
		self._action = kwargs["action"]
		self._ip = kwargs["ip"]
		self._proto = kwargs["proto"]
		self._options = kwargs["options"]

		config = {}
		self.filename = NETPLAN_CFG_DIR + NETPLAN_CFG_PREFIX + self._ifname + ".yaml"

		self.__load()

	def __generate_skeleton_config(self):
		"""
		Generate skeleton config
		"""
		self.config = {'network': {"version": 2, 'ethernets': {self._ifname: {}}}}

	def __set_gateway(self):
		"""
		set_gateway action implementation for netplan config
		"""
		ifcfg = self.config["network"]["ethernets"][self._ifname]
		for ip in self._ip.split():
			if 'remove' in ip:
				continue

			gw_proto = "gateway4"
			if is_ip_proto(ip, 6):
				gw_proto = "gateway6"

			if "routes" in ifcfg:
				ifcfg.pop("routes")

			ifcfg[gw_proto] = ip

	def __set_dhcp(self):
		"""
		set_dhcp action implementation for netplan config
		TODO: its necessary to decide what to do with this function.
		Original shell script removes configuration entirely and rewrites it
		when dhcp is set. While it should be reasonable just to remove relevant
		protocol (ipv4/ipv6) configuration and set dhcp.
		For the sake of compatibility, function replicates existing behavior,
		wipes configuration file and just sets dhcp.
		"""
		self.__generate_skeleton_config()

		ifcfg = self.config["network"]["ethernets"][self._ifname]

		for proto in list(self._proto):
			if proto == '4':
				ifcfg["dhcp4"] = "yes"
			if proto == '6':
				ifcfg["dhcp6"] = "yes"

	def __set_route(self):
		"""
		set_dhcp action implementation for netplan config
		"""
		# Create empty list if it is missing to avoid KeyValue exceptions
		if "routes" not in self.config["network"]["ethernets"][self._ifname]:
			self.config["network"]["ethernets"][self._ifname]["routes"] = []

		# Take subtree pointer to shorten and simplify further code
		route_tree = self.config["network"]["ethernets"][self._ifname]["routes"]

		if self._ip == 'remove':
			for route in route_tree:
				if is_ip_proto(route["to"], 4):
						route_tree.remove(route)

		elif self._ip == 'remove6':
			for route in route_tree:
				if is_ip_proto(route["to"], 6):
						route_tree.remove(route)

		else:
			to, via, metric = split_route(self._ip)
			if metric:
				route = {"to": to, "via": via, "metric": metric}
			else:
				route = {"to": to, "via": via}

			# Check for duplicates before adding route
			if route not in route_tree:
				route_tree.append(route)

	def __set_ip(self):
		"""
		set_ip action implementation for netplan config
		IMPORTANT: on each use old config is flushed. That is to ensure
		backward-compatibility. prl_nettool supplies full list of IPs on each
		set_ip invocation
		"""
		self.__generate_skeleton_config()
		ifcfg = self.config["network"]["ethernets"][self._ifname]

		for ip in self._ip.split():
			if self._ip == 'remove' or self._ip == 'remove6':
				continue

			if "addresses" not in ifcfg:
				ifcfg["addresses"] = []

			# This is necessary for compatibility, we must replicate old behavior
			if "/" not in list(ip):
				if is_ip_proto(ip, 4):
					ip += "/32"
				else:
					ip += "/64"
			else:
				if is_ip_proto(ip, 4):
					addr, netmask = ip.split("/")
					if "." in netmask:
						netmask = netmask_to_cidr(netmask)
						ip = addr + "/" + str(netmask)

			ifcfg["addresses"].append(ip)

		ifcfg["dhcp4"] = False
		ifcfg["dhcp6"] = False
		for opt in self._options.split():
			if opt =="dhcp":
				ifcfg["dhcp4"] = True
			if opt =="dhcp6":
				fcfg["dhcp6"] = True

	def __restart(self):
		"""
		restart action implementation for netplan config
		"""
		p = subprocess.Popen("netplan generate".split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		out = p.communicate()

		if p.returncode == 0:
			p = subprocess.Popen("netplan apply".split())
			out = p.communicate()

			if p.returncode:
				print("netplan apply failed [%d].\nstdout:%s\nstderr:%s\n" %
					(p.returncode, str(out[0]), str(out[1])))
		else:
			print("netplan generate failed [%d].\nstdout:%s\nstderr:%s\n" %
				(p.returncode, str(out[0]), str(out[1])))

		return p.returncode

	def __get_dhcp(self):
		"""
		get_dhcp action implementation for netplan config
		"""
		ifcfg = self.config["network"]["ethernets"][self._ifname]

		if self._proto == 6:
			dhcpvp = "dhcp6"
		else:
			dhcpvp = "dhcp4"

		if dhcpvp in ifcfg:
			if ifcfg[dhcpvp]:
				return 0
			else:
				return 1
		else:
			return 2

	def __load(self):
		"""
		Read configuration file from disk. If it is missing - construct skeleton config
		"""
		if os.path.exists(self.filename):
			with open(self.filename) as f:
				self.config = yaml.load(f.read())
		else:
			self.__generate_skeleton_config()

	def __save(self):
		"""
		Write config to the disk
		"""
		with open(self.filename + ".tmp", "w") as f:
			f.write(yaml.dump(self.config, default_flow_style=False))
		if os.path.exists(self.filename):
			os.rename(self.filename, self.filename + ".bkp")
		os.rename (self.filename + ".tmp", self.filename)

	def perform_action(self):
		"""
		Perform action over the config file
		"""
		if self._action == "get_dhcp":
			return self.__get_dhcp()

		if self._action == "restart":
			return self.__restart()

		if self._action == "set_dhcp":
			self.__set_dhcp()
		elif self._action == "set_route":
			self.__set_route()
		elif self._action == "set_ip":
			self.__set_ip()
		elif self._action == "set_gateway":
			self.__set_gateway()

		self.__save()
		self.__restart()


"""
Main body starts here
"""
if __name__ == '__main__':
	args = getArgParser().parse_args()

	npcfg = npConfig(action=args.action, device=args.device,
		ip=args.ip, proto=args.proto, options=args.options)

	res = npcfg.perform_action()

	sys.exit(res)
