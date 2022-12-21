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

#include "rcconf.h"
#include "rcconf_sublist.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int load_item(struct rcconf_sublist *sublist, struct rcconf *cfg, const char *key);
static void load_clean(struct rcconf_sublist *sublist, struct rcconf *cfg);

void rcconf_sublist_init(struct rcconf_sublist *sublist, const char *key,
						 const char *item_prefix, const char *item_key)
{
	if ((!sublist) || (!key) || (!item_prefix) || (!item_key)) {
		return;
	}

	sublist->key = key;
	sublist->item_prefix = item_prefix;
	sublist->item_key = item_key;
	sublist->size = 0;

	rcconf_list_init(&sublist->list);
}

int rcconf_sublist_append(struct rcconf_sublist *sublist, const char *val)
{
	struct rcconf_sublist_item *item;

	if (!sublist) {
		return -EINVAL;
	}

	item = malloc(sizeof(*item));
	if (!item) {
		return -ENOMEM;
	}

	if (val) {
		item->val = strdup(val);
		if (item->val == NULL) {
			free(item);
			return -ENOMEM;
		}
	}

	rcconf_list_append(&sublist->list, &item->list);

	sublist->size++;

	return 0;
}

int rcconf_sublist_set(struct rcconf_sublist *sublist, const char *val)
{
	if ((!sublist) || (!val)) {
		return -EINVAL;
	}

	rcconf_sublist_del_item_by_val(sublist, val);

	return rcconf_sublist_append(sublist, val);
}

void rcconf_sublist_item_free(struct rcconf_sublist_item *item)
{
	if (!item) {
		return;
	}
	free((void *)item->val);
	free(item);
}

void rcconf_sublist_del_item(struct rcconf_sublist *sublist, struct rcconf_sublist_item *item)
{
	if ((!sublist) || (!item)) {
		return;
	}
	rcconf_list_del(&item->list);
	rcconf_sublist_item_free(item);
	sublist->size--;
}

struct rcconf_sublist_item *rcconf_sublist_get_item_by_val(struct rcconf_sublist *sublist, const char *val)
{
	struct rcconf_sublist_item *item;

	if ((!sublist) || (!val)) {
		errno = EINVAL;
		return NULL;
	}

	rcconf_sublist_foreach(sublist, item) {
		if (strcmp(item->val, val) == 0) {
			return item;
		}
	}

	errno = ENOENT;
	return NULL;
}

int rcconf_sublist_del_item_by_val(struct rcconf_sublist *sublist, const char *val)
{
	struct rcconf_sublist_item *item;

	item = rcconf_sublist_get_item_by_val(sublist, val);
	if (!item) {
		return -errno;
	}

	rcconf_sublist_del_item(sublist, item);

	return 0;
}

void rcconf_sublist_free(struct rcconf_sublist *sublist)
{
	struct rcconf_sublist_item *item;
	struct rcconf_list *list_item, *list_next;

	if (!sublist) {
		return;
	}

	for (list_item = sublist->list.next; list_item != &sublist->list; list_item = list_next) {
		list_next = list_item->next;
		item = container_of(list_item, struct rcconf_sublist_item, list);
		rcconf_sublist_item_free(item);
	}

	rcconf_sublist_init(sublist, sublist->key, sublist->item_prefix, sublist->item_key);
}

int rcconf_sublist_load(struct rcconf_sublist *sublist, struct rcconf *cfg)
{
	struct rcconf_item *item;
	char *keys, *key, *end;
	size_t len;
	int res;

	if ((!sublist) || (!cfg)) {
		return -EINVAL;
	}

	item = rcconf_get_item(cfg, sublist->key);
	if ((!item) || (!item->val)) {
		return -EINVAL;
	}

	keys = strdup(item->val);
	if (keys == NULL) {
		return -ENOMEM;
	}

	for (key = keys; key; ) {
		end = strchr(key, ' ');

		if (end != NULL) {
			*end = '\0';
		}

		len = strlen(key);
		if (len > 0) {
			res = load_item(sublist, cfg, key);
			if (res != 0) {
				rcconf_sublist_free(sublist);
				free(keys);
				return res;
			}
		}

		if (end == NULL) {
			break;
		}

		key += len + 1;
	}

	free(keys);

	load_clean(sublist, cfg);

	return 0;
}

int rcconf_sublist_save(struct rcconf_sublist *sublist, struct rcconf *cfg)
{
	char keys[RCCONF_SUBLIST_KEY_SET_MAX_LEN];
	char key[RCCONF_SUBLIST_ITEM_KEY_MAX_LEN];
	char *p, *tmpl;
	struct rcconf_sublist_item *item;
	unsigned int i;
	int len, res;

	if ((!sublist) || (!cfg)) {
		return -EINVAL;
	}

	i = 0;
	p = keys;
	rcconf_sublist_foreach(sublist, item) {
		len = sizeof(keys) - (p - keys);
		tmpl = (i == 0) ? "%s%d" : " %s%u";

		res = snprintf(p, len, tmpl, sublist->item_key, i);
		if (res >= len) {
			return -EINVAL;
		}

		p += res;

		res = snprintf(key, sizeof(key), "%s%s%u", sublist->item_prefix, sublist->item_key, i);
		if (res >= len) {
			return -EINVAL;
		}

		i++;

		res = rcconf_set_item(cfg, key, item->val);
		if (res != 0) {
			return res;
		}
	}

	return rcconf_set_item(cfg, sublist->key, keys);
}

struct rcconf_sublist_item *rcconf_sublist_next(struct rcconf_sublist *sublist, struct rcconf_sublist_item *item)
{
	if (item->list.next == &sublist->list) {
		return NULL;
	}
	return container_of(item->list.next, struct rcconf_sublist_item, list);
}

struct rcconf_sublist_item *rcconf_sublist_first(struct rcconf_sublist *sublist)
{
	if (sublist->list.next == &sublist->list) {
		return NULL;
	}
	return container_of(sublist->list.next, struct rcconf_sublist_item, list);
}

static int load_item(struct rcconf_sublist *sublist, struct rcconf *cfg, const char *key)
{
	char full_key[RCCONF_SUBLIST_ITEM_KEY_MAX_LEN];
	struct rcconf_item *item;
	int res;

	res = snprintf(full_key, sizeof(full_key), "%s%s", sublist->item_prefix, key);
	if (res >= sizeof(full_key)) {
		return -EINVAL;
	}

	item = rcconf_get_item(cfg, full_key);
	if (item == NULL) {
		return -ENOENT;
	}

	return rcconf_sublist_append(sublist, item->val);
}

static void load_clean(struct rcconf_sublist *sublist, struct rcconf *cfg)
{
	struct rcconf_list *list_item, *list_next;
	struct rcconf_item *item;

	for (list_item = cfg->list.next; list_item != &cfg->list; list_item = list_next) {
		list_next = list_item->next;
		item = container_of(list_item, struct rcconf_item, list);
		if (strncmp(item->key, sublist->item_prefix, strlen(sublist->item_prefix)) == 0) {
			rcconf_list_del(list_item);
			rcconf_free_item(item);
		}
	}
}
