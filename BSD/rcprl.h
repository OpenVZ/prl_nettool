/*
 * Copyright (c) 2022 Virtuozzo International GmbH. All rights reserved.
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
 * Our contact details: Virtuozzo International GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 */

#ifndef __RCPRL_H__
#define __RCPRL_H__

#include "rcconf.h"
#include "rcconf_sublist.h"

#define RC_PATH		"/etc/rc.conf"
#define RCPRL_PATH	"/etc/rc.prl_nettool"

#define RCPRL_HEADER	"### This file was created automatically by prl_nettool. ###\n" \
						"### Don't change any line of the file.                  ###\n"

#define rcprl_save_items(...) rcconf_save_items(RCPRL_PATH, RCPRL_HEADER, __VA_ARGS__);

extern int rcprl_init(void);
extern void rcprl_free(void);
extern int rcprl_load(void);
extern int rcprl_save(void);
extern int rcprl_sublist_load(struct rcconf_sublist *sublist);
extern int rcprl_sublist_save(struct rcconf_sublist *sublist);
extern struct rcconf_item *rcprl_get_item(const char *key);

#endif
