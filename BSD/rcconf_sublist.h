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

#ifndef __RCCONF_SUBLIST_H__
#define __RCCONF_SUBLIST_H__

#include "rcconf_list.h"
#include <stddef.h>

struct rcconf_sublist_item {
	const char *val;
	struct rcconf_list list;
};

struct rcconf_sublist {
	size_t size;
	const char *key;
	const char *item_prefix;
	const char *item_key;
	struct rcconf_list list;
};


#define RCCONF_SUBLIST_ITEM_KEY_MAX_LEN 256
#define RCCONF_SUBLIST_KEY_SET_MAX_LEN  4096

#define rcconf_sublist_foreach(sublist, item) \
	for (item = rcconf_sublist_first(sublist); (item); item = rcconf_sublist_next(sublist, item))

extern void rcconf_sublist_init(struct rcconf_sublist *sublist, const char *key,
								const char *item_prefix, const char *item_key);
extern int rcconf_sublist_append(struct rcconf_sublist *sublist, const char *val);
extern int rcconf_sublist_set(struct rcconf_sublist *sublist, const char *val);
extern void rcconf_sublist_item_free(struct rcconf_sublist_item *item);
extern void rcconf_sublist_del_item(struct rcconf_sublist *sublist, struct rcconf_sublist_item *item);
extern struct rcconf_sublist_item *rcconf_sublist_get_item_by_val(struct rcconf_sublist *sublist,
																  const char *val);
extern int rcconf_sublist_del_item_by_val(struct rcconf_sublist *sublist, const char *val);
extern void rcconf_sublist_free(struct rcconf_sublist *sublist);
extern int rcconf_sublist_load(struct rcconf_sublist *sublist, struct rcconf *cfg);
extern int rcconf_sublist_save(struct rcconf_sublist *sublist, struct rcconf *cfg);
extern struct rcconf_sublist_item *rcconf_sublist_next(struct rcconf_sublist *sublist,
													   struct rcconf_sublist_item *item);
extern struct rcconf_sublist_item *rcconf_sublist_first(struct rcconf_sublist *sublist);

#endif
