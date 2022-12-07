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
static struct rcconf_field *parse_line(char *line);
static void strip(char *str);
static bool is_space(char c);

int rcconf_save_fields(const char *key, const char *val, ...)
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
			res = rcconf_del_field(&cfg, key);
			if (res != 0 && res != -ENOENT) {
				werror("ERROR: Can't remove %s field", key);
				goto err;
			}
		} else {
			res = rcconf_set_field(&cfg, key, val);
			if (res != 0) {
				werror("ERROR: Can't set %s field", key);
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
	cfg->fields.next = &cfg->fields;
	cfg->fields.prev = &cfg->fields;
}

void rcconf_free(struct rcconf *cfg)
{
	struct rcconf_field *field, *next;

	if (!cfg) {
		return;
	}

	next = NULL;
	for (field = cfg->fields.next; field != &cfg->fields; field = next) {
		next = field->next;
		rcconf_free_field(field);
	}

	cfg->fields.prev = &cfg->fields;
	cfg->fields.next = &cfg->fields;
}

int rcconf_load(struct rcconf *cfg)
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	struct rcconf_field *field;

	if (!cfg) {
		return -EINVAL;
	}

	rcconf_free(cfg);

	fp = fopen(RC_PRL_PATH, "r");
	if (!fp)
		return 0; /* If there is no config file, assume the config is empty */

	while ((read = getline(&line, &len, fp)) != -1) {
		field = parse_line(line);
		rcconf_add_field(cfg, field);
	}
	free(line);
	fclose(fp);

	return 0;
}

int rcconf_save(struct rcconf *cfg)
{
	FILE *fp;
	struct rcconf_field *field;
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

	RC_CONF_FOREACH_FIELD(cfg, field) {
		fprintf(fp, "%s=\"%s\"\n", field->key, field->val);
	}
	fclose(fp);

	return 0;
}

struct rcconf_field *rcconf_get_field(struct rcconf *cfg, const char *key)
{
	struct rcconf_field *field;

	if ((!cfg) || (!key)) {
		return NULL;
	}

	RC_CONF_FOREACH_FIELD(cfg, field) {
		if (strcmp(field->key, key) == 0) {
			return field;
		}
	}

	return NULL;
}

int rcconf_set_field(struct rcconf *cfg, const char *key, const char *val)
{
	struct rcconf_field *field;
	char *tmp;

	if ((!cfg) || (!key) || (!val)) {
		return -EINVAL;
	}

	field = rcconf_get_field(cfg, key);
	if (field) {
		tmp = strdup(val);
		if (!tmp) {
			return -ENOMEM;
		}

		free(field->val);
		field->val = tmp;
		return 0;
	}

	field = rcconf_make_field(key, val);
	if (!field) {
		return -errno;
	}

	return rcconf_add_field(cfg, field);
}

int rcconf_del_field(struct rcconf *cfg, const char *key)
{
	struct rcconf_field *field;

	if ((!cfg) || (!key)) {
		return -EINVAL;
	}

	field = rcconf_get_field(cfg, key);
	if (!field) {
		return -ENOENT;
	}

	field->next->prev = field->prev;
	field->prev->next = field->next;
	return 0;
}

int rcconf_add_field(struct rcconf *cfg, struct rcconf_field *field)
{
	struct rcconf_field *check;

	if ((!cfg) || (!field)) {
		return -EINVAL;
	}

	check = rcconf_get_field(cfg, field->key);
	if (check) {
		return -EEXIST;
	}

	field->prev = cfg->fields.prev;
	field->next = &cfg->fields;
	cfg->fields.prev->next = field;
	cfg->fields.prev = field;

	return 0;
}

struct rcconf_field *rcconf_make_field(const char *key, const char *val)
{
	struct rcconf_field *field;

	if ((!key) || (!val)) {
		return NULL;
	}

	field = malloc(sizeof(*field));
	if (!field) {
		errno = -ENOMEM;
		return NULL;
	}

	field->next = NULL;
	field->key = strdup(key);
	field->val = strdup(val);

	if ((!field->key) || (!field->val)) {
		rcconf_free_field(field);
		errno = -ENOMEM;
		return NULL;
	}

	return field;
}

void rcconf_free_field(struct rcconf_field *field)
{
	if (!field) {
		return;
	}
	free(field->key);
	free(field->val);
	free(field);
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

static struct rcconf_field *parse_line(char *line)
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

	return rcconf_make_field(key, val);
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
