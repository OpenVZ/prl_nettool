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
 * Our contact details: Parallels International GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 *
 * header for  methods for getting distribution
 */

#ifndef __DETECTION_H__
#define __DETECTION_H__

enum OS_VENDOR {
	VENDOR_UNKNOWN = 0,
	VENDOR_REDHAT = 1, /*redhat, fedora, centos*/
	VENDOR_SUSE = 2, /*suse, sles*/
	VENDOR_DEBIAN = 3 /*debian, ubuntu*/
};

int get_distribution(int *os_vendor, char **os_script_prefix);

#endif
