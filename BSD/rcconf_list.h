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

#ifndef __RCCONF_LIST_H__
#define __RCCONF_LIST_H__

#include <stddef.h>

struct rcconf_list {
	struct rcconf_list *prev;
	struct rcconf_list *next;
};

#define container_of(ptr, type, member) ({ \
	const typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member)); })

#define rcconf_list_foreach(list, item) \
	for (item = (list)->next; (item) != (list); item = (item)->next)

extern void rcconf_list_init(struct rcconf_list *list);
extern void rcconf_list_append(struct rcconf_list *list, struct rcconf_list *item);
extern void rcconf_list_del(struct rcconf_list *item);

#endif
