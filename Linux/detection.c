/*
 * Copyright (c) 2015-2017, Parallels International GmbH
 *
 * This file is part of OpenVZ. OpenVZ is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * Lesser General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Our contact details: Parallels IP Holdings GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 *
 * methods for getting distribution
 */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <string.h>

#include "detection.h"

#define RH_RELEASE "/etc/redhat-release"
#define SUSE_RELEASE "/etc/SuSE-release"
#define DEBIAN_RELEASE "/etc/debian_version"

int get_distribution(int *os_vendor, char **os_script_prefix)
{
	struct stat st;

	if (os_vendor == NULL)
		return -1; /*nothing to do*/

	//TODO: add detection distrib
	if (os_vendor)
		*os_vendor = VENDOR_UNKNOWN;
	if (os_script_prefix)
		*os_script_prefix = NULL;

	if (!stat(RH_RELEASE, &st)) {
		if (os_vendor)
			*os_vendor = VENDOR_REDHAT;
		if (os_script_prefix)
			*os_script_prefix = "redhat";
	} else if (!stat(SUSE_RELEASE, &st)) {
		if (os_vendor)
			*os_vendor = VENDOR_SUSE;
		if (os_script_prefix)
			*os_script_prefix = "suse";
	} else if (!stat(DEBIAN_RELEASE, &st)) {
		if (os_vendor)
			*os_vendor = VENDOR_DEBIAN;
		if (os_script_prefix)
			*os_script_prefix = "debian";
	} else
		return -1;

	return 0;
}
