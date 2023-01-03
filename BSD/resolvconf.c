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

#include "resolvconf.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#define MAX_SEARCH_LIST_LEN 256
#define MAX_LINES           256

#define DNS_TMPL         "nameserver %s"
#define DNS_TMPL_SCAN    " nameserver %46s"
#define SEARCH_TMPL      "search %s"
#define SEARCH_TMPL_SCAN " search %[^\n]s"


static int add_line(struct resolvconf_line *head, const char *str);

void resolvconf_init(struct resolvconf_line *head)
{
	if (!head)
		return;

	head->data = NULL;
	head->next = NULL;
}

int resolvconf_load__(struct resolvconf_line *head, const char *path)
{
	FILE *f;
	char *str = NULL;
	size_t len = 0;
	int res;

	if (!head || !path)
		return -EINVAL;

	resolvconf_init(head);

	f = fopen(path, "r");
	if (f == NULL)
		return -errno;

	while (getline(&str, &len, f) >= 0) {
		res = add_line(head, str);
		if (res != 0) {
			resolvconf_free(head);
			return res;
		}
		head = head->next;
	}

	fclose(f);

	return 0;
}

int resolvconf_save__(struct resolvconf_line *head, const char *path)
{
	struct resolvconf_line *line;
	FILE *f;

	if (!head || !path)
		return -EINVAL;

	f = fopen(path, "w");
	if (f == NULL)
		return -errno;

	for (line = head->next; line; line = line->next)
		fprintf(f, "%s", line->data);

	return fclose(f);
}

void resolvconf_free(struct resolvconf_line *head)
{
	struct resolvconf_line *line, *next;

	if (!head)
		return;

	for (line = head->next; line; line = next) {
		next = line->next;
		free(line->data);
		free(line);
	}
}

int resolvconf_set_namserver(struct resolvconf_line *head, const char *dns)
{
	struct resolvconf_line *line, *last;
	char buf[INET6_ADDRSTRLEN + sizeof(DNS_TMPL)];
	int i;

	if ((!head) || (!dns))
		return -EINVAL;

	if (strlen(dns) > INET6_ADDRSTRLEN)
		return -EINVAL;

	last = head;
	i = 0;
	for (line = head->next; line; line = line->next) {
		i++;
		last = line;

		if (sscanf(line->data, DNS_TMPL_SCAN, buf) != 1)
			continue;

		if (strcmp(buf, dns) == 0)
			return 0;

		if (i >= MAX_LINES)
			return -ERANGE;
	}

	snprintf(buf, sizeof(buf), DNS_TMPL "\n", dns);

	return add_line(last, buf);
}

const char *resolvconf_get_search_list(struct resolvconf_line *head)
{
	struct resolvconf_line *line;
	static char buf[MAX_SEARCH_LIST_LEN + sizeof(SEARCH_TMPL)];

	if (!head)
		return NULL;

	for (line = head->next; line; line = line->next) {
		if (sscanf(line->data, SEARCH_TMPL_SCAN, buf) == 1)
			return buf;
	}

	return NULL;
}

int resolvconf_set_search_list(struct resolvconf_line *head, const char *list)
{
	struct resolvconf_line *line, *last;
	char buf[MAX_SEARCH_LIST_LEN + sizeof(SEARCH_TMPL)];
	char *p;

	if ((!head) || (!list))
		return -EINVAL;

	if (strlen(list) >= MAX_SEARCH_LIST_LEN)
		return -EINVAL;

	last = head;
	for (line = head->next; line; line = line->next) {
		last = line;

		if (sscanf(line->data, SEARCH_TMPL_SCAN, buf) != 1)
			continue;

		snprintf(buf, sizeof(buf), SEARCH_TMPL "\n", list);

		p = strdup(buf);
		if (p == NULL)
			return -ENOMEM;

		free(line->data);
		line->data = p;
		return 0;
	}

	snprintf(buf, sizeof(buf), SEARCH_TMPL "\n", list);
	return add_line(last, buf);
}

struct resolvconf_line *resolvconf_get_next_dns(struct resolvconf_line *line, const char **dns)
{
	static char buf[INET6_ADDRSTRLEN + 1];

	for (line = line->next; line; line = line->next) {
		if (sscanf(line->data, DNS_TMPL_SCAN, buf) == 1) {
			*dns = buf;
			return line;
		}
	}
	*dns = NULL;
	return NULL;
}

static int add_line(struct resolvconf_line *head, const char *str)
{
	struct resolvconf_line *line;

	line = malloc(sizeof(*line));
	if (line == NULL)
		return -ENOMEM;

	line->data = strdup(str);
	if (line->data == NULL) {
		free(line);
		return -ENOMEM;
	}

	line->next = NULL;
	head->next = line;

	return 0;
}
