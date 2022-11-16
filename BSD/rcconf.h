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

struct rcconf_field {
    char *key;
    char *val;
    struct rcconf_field *next;
    struct rcconf_field *prev;
};

struct rcconf {
    struct rcconf_field fields;
};

#define RC_CONF_FOREACH_FIELD(cfg, field) \
    for (field = (cfg)->fields.next; (field) != &(cfg)->fields; field = (field)->next)

/* You can call this function with any number of key/val. You must
 * terminate argumnets with NULL. There should be an even number of
 * arguments, excluding terminating NULL.
 *
 * Example: rcconf_save_fields("key1", "val1, "key2", "val2, NULL);
 */
extern int rcconf_save_fields(const char *key, const char *val, ...);

extern void rcconf_init(struct rcconf *cfg);
extern void rcconf_free(struct rcconf *cfg);
extern int rcconf_load(struct rcconf *cfg);
extern int rcconf_save(struct rcconf *cfg);
extern struct rcconf_field *rcconf_get_field(struct rcconf *cfg, const char *key);
extern int rcconf_set_field(struct rcconf *cfg, const char *key, const char *val);
extern int rcconf_del_field(struct rcconf *cfg, const char *key);
extern int rcconf_add_field(struct rcconf *cfg, struct rcconf_field *field);
extern struct rcconf_field *rcconf_make_field(const char *key, const char *val);
extern void rcconf_free_field(struct rcconf_field *field);

#endif
