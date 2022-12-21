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

#ifndef __RCCONF_H__
#define __RCCONF_H__

struct rcconf_item {
    char *key;
    char *val;
    struct rcconf_item *next;
    struct rcconf_item *prev;
};

struct rcconf {
    struct rcconf_item items;
};

#define RC_CONF_FOREACH_FIELD(cfg, item) \
    for (item = (cfg)->items.next; (item) != &(cfg)->items; item = (item)->next)

/* You can call this function with any number of key/val. You must
 * terminate argumnets with NULL. There should be an even number of
 * arguments, excluding terminating NULL.
 * Also you can remove some items by setting NULL for val argument.
 * Note that you must add teminating NULL in any case.
 *
 * Example:
 *     rcconf_save_items("/etc/rc.conf", "header",
 *                        "key1", "val1, "key2", "val2", "rm_key3", NULL, NULL);
 *                        |_______ add key/val _______|  |__rm key/val_|  |_teminator
 */
extern int rcconf_save_items(const char *key, const char *val, ...);

extern void rcconf_init(struct rcconf *cfg);
extern void rcconf_free(struct rcconf *cfg);
extern int rcconf_load(struct rcconf *cfg);
extern int rcconf_save(struct rcconf *cfg);
extern struct rcconf_item *rcconf_get_item(struct rcconf *cfg, const char *key);
extern int rcconf_set_item(struct rcconf *cfg, const char *key, const char *val);
extern int rcconf_del_item(struct rcconf *cfg, const char *key);
extern int rcconf_add_item(struct rcconf *cfg, struct rcconf_item *item);
extern struct rcconf_item *rcconf_make_item(const char *key, const char *val);
extern void rcconf_free_item(struct rcconf_item *item);

#endif
