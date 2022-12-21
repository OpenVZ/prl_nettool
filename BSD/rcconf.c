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
#include "../common.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>

#define RC_PATH		"/etc/rc.conf"
#define RC_PRL_PATH	"/etc/rc.prl_nettool"

#define RC_HEADER_FIRST	"### The section below was created automatically by prl_nettool. ###\n"
#define RC_HEADER_REST	"### Don't change any line of the section and don't put any      ###\n" \
						"### variables below the section.                                ###\n"
#define RC_BODY			"if [ -r " RC_PRL_PATH     " ]; then                             ###\n" \
						"    . "   RC_PRL_PATH   "                                       ###\n" \
						"fi                                                              ###\n"
#define RC_END			"###################################################################\n"

#define PRL_HEADER	"### This file was created automatically by prl_nettool. ###\n" \
					"### Don't change any line of the file.                  ###\n"

static int handle_rc_conf(void);
static struct rcconf_item *parse_line(char *line);
static void strip(char *str);
static bool is_space(char c);

int rcconf_save_items(const char *key, const char *val, ...)
{
	struct rcconf cfg;
	va_list args;
	int res;

	rcconf_init(&cfg);

	res = rcconf_load(&cfg);
	if (res != 0) {
		werror("ERROR: Can't load");
		return res;
	}

	va_start(args, val);

	while (key != NULL) {
		if (val == NULL) {
			res = rcconf_del_item(&cfg, key);
			if (res != 0 && res != -ENOENT) {
				werror("ERROR: Can't remove %s item", key);
				goto err;
			}
		} else {
			res = rcconf_set_item(&cfg, key, val);
			if (res != 0) {
				werror("ERROR: Can't set %s item", key);
				goto err;
			}
		}

		key = va_arg(args, const char *);
		if (key != NULL)
			val = va_arg(args, const char *);
	}

	va_end(args);

	res = rcconf_save(&cfg);
	if (res != 0)
		werror("ERROR: Can't save to rc.conf");

err:
	rcconf_free(&cfg);

	return res;
}

void rcconf_init(struct rcconf *cfg)
{
	if (!cfg) {
		return;
	}
	rcconf_list_init(&cfg->list);
}

void rcconf_free(struct rcconf *cfg)
{
	struct rcconf_list *list, *item, *next;

	if (!cfg) {
		return;
	}

	list = &cfg->list;
	next = NULL;
	for (item = list->next; item != list; item = next) {
		next = item->next;
		rcconf_free_item(container_of(item, struct rcconf_item, list));
	}

	rcconf_list_init(list);
}

int rcconf_load(struct rcconf *cfg)
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	struct rcconf_item *item;

	if (!cfg) {
		return -EINVAL;
	}

	rcconf_free(cfg);

	fp = fopen(RC_PRL_PATH, "r");
	if (!fp)
		return 0; /* If there is no config file, assume the config is empty */

	while ((read = getline(&line, &len, fp)) != -1) {
		item = parse_line(line);
		rcconf_add_item(cfg, item);
	}
	free(line);
	fclose(fp);

	return 0;
}

int rcconf_save(struct rcconf *cfg)
{
	FILE *fp;
	struct rcconf_list *list_item;
	struct rcconf_item *item;
	int res;

	if (!cfg) {
		return -EINVAL;
	}

	res = handle_rc_conf();
	if (res != 0)
		return res;

	fp = fopen(RC_PRL_PATH, "w+");
	if (!fp)
		return -errno;

	fprintf(fp, "%s\n", PRL_HEADER);

	rcconf_list_foreach(&cfg->list, list_item) {
		item = container_of(list_item, struct rcconf_item, list);
		fprintf(fp, "%s=\"%s\"\n", item->key, item->val);
	}
	fclose(fp);

	return 0;
}

struct rcconf_item *rcconf_get_item(struct rcconf *cfg, const char *key)
{
	struct rcconf_list *list_item;
	struct rcconf_item *item;

	if ((!cfg) || (!key)) {
		return NULL;
	}

	rcconf_list_foreach(&cfg->list, list_item) {
		item = container_of(list_item, struct rcconf_item, list);
		if (strcmp(item->key, key) == 0) {
			return item;
		}
	}

	return NULL;
}

int rcconf_set_item(struct rcconf *cfg, const char *key, const char *val)
{
	struct rcconf_item *item;
	char *tmp;

	if ((!cfg) || (!key) || (!val)) {
		return -EINVAL;
	}

	item = rcconf_get_item(cfg, key);
	if (item) {
		tmp = strdup(val);
		if (!tmp) {
			return -ENOMEM;
		}

		free(item->val);
		item->val = tmp;
		return 0;
	}

	item = rcconf_make_item(key, val);
	if (!item) {
		return -errno;
	}

	return rcconf_add_item(cfg, item);
}

int rcconf_del_item(struct rcconf *cfg, const char *key)
{
	struct rcconf_item *item;

	if ((!cfg) || (!key)) {
		return -EINVAL;
	}

	item = rcconf_get_item(cfg, key);
	if (!item) {
		return -ENOENT;
	}

	rcconf_list_del(&item->list);

	return 0;
}

int rcconf_add_item(struct rcconf *cfg, struct rcconf_item *item)
{
	struct rcconf_item *check;

	if ((!cfg) || (!item)) {
		return -EINVAL;
	}

	check = rcconf_get_item(cfg, item->key);
	if (check) {
		return -EEXIST;
	}

	rcconf_list_append(&cfg->list, &item->list);
	return 0;
}

struct rcconf_item *rcconf_make_item(const char *key, const char *val)
{
	struct rcconf_item *item;

	if ((!key) || (!val)) {
		return NULL;
	}

	item = malloc(sizeof(*item));
	if (!item) {
		errno = -ENOMEM;
		return NULL;
	}

	rcconf_list_init(&item->list);
	item->key = strdup(key);
	item->val = strdup(val);

	if ((!item->key) || (!item->val)) {
		rcconf_free_item(item);
		errno = -ENOMEM;
		return NULL;
	}

	return item;
}

void rcconf_free_item(struct rcconf_item *item)
{
	if (!item) {
		return;
	}
	free(item->key);
	free(item->val);
	free(item);
}

static int handle_rc_conf(void)
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	fp = fopen(RC_PATH, "r+");
	if (!fp) {
		return -errno;
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		if (strcmp(line, RC_HEADER_FIRST) == 0) {
			fclose(fp);
			return 0;
		}
	}

	fprintf(fp, "\n%s%s", RC_HEADER_FIRST, RC_HEADER_REST);
	fprintf(fp, "%s%s", RC_BODY, RC_END);

	fclose(fp);
	return 0;
}

static struct rcconf_item *parse_line(char *line)
{
	char *p, *key, *val, *end;

	p = strchr(line, '=');
	if (!p) {
		return NULL;
	}

	*p = '\0';
	key = line;
	val = p + 1;

	strip(key);
	if (strlen(key) == 0) {
		return NULL;
	}

	strip(val);
	if (strlen(val) < 2) {
		return NULL;
	}

	end = val + strlen(val) - 1;
	if (*val != '"' || *end != '"') {
		return NULL;
	}
	val++;
	*end = '\0';

	return rcconf_make_item(key, val);
}

static void strip(char *str)
{
	char *beg, *end;

	beg = str;
	end = beg + strlen(str);

	for (; beg != end && is_space(*beg); beg++);
	for (; beg != end && is_space(*end); end--);

	memmove(str, beg, end - beg + 1);
	str[end - beg + 1] = '\0';
}

static bool is_space(char c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\0');
}
