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
 * common functions of tool
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef _WIN_ //Windows

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

#include <tchar.h>
#include <sdkddkver.h>
#else //Linux

// TODO
// Bad thing to hardcode path to scripts directory
// For example, this prevents us to install into /usr/lib32
// on 64-bit Debians
#define SCRIPT_DIR	"/usr/lib/parallels-tools/tools/scripts"

#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN_

#ifdef strdup
#undef strdup
#endif
#define strdup _strdup

#ifdef snprintf
#undef snprintf
#endif
#define snprintf _snprintf

#ifndef strcasecmp
#define strcasecmp _stricmp
#endif

#ifndef strcasestr
#define strcasestr StrStrIA
#endif
#include <shlwapi.h>

#endif

void error(int err, const char *fmt, ...);
void debug(const char *fmt, ...);

#ifndef _WIN_
#define werror(fmt, args...)            error(0, fmt, ##args)
#endif

/* named VARUNUSED to avoid conflict with parallelstypes.h */
#define VARUNUSED(expr) do { (void)(expr); } while (0)

int is_ipv6(const char *ip);
int is_ipv6_supported();

struct route
{
	char *ip;
	char *gw;
	char *metric;
};

void parse_route(char *value, struct route *route);
void clear_route(struct route *route);

#endif
