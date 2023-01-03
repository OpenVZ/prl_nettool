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

#ifndef __RESOLVCONF_H__
#define __RESOLVCONF_H__

struct resolvconf_line {
	char *data;
	struct resolvconf_line *next;
};

#define RESOLVCONF_PATH "/etc/resolv.conf"

#define resolvconf_dns_foreach(head, dns) for (struct resolvconf_line *line = resolvconf_get_next_dns(head, &dns); line; line = resolvconf_get_next_dns(line, &dns))

extern void resolvconf_init(struct resolvconf_line *head);
extern int resolvconf_load__(struct resolvconf_line *head, const char *path);
extern int resolvconf_save__(struct resolvconf_line *head, const char *path);
extern void resolvconf_free(struct resolvconf_line *head);
extern int resolvconf_set_namserver(struct resolvconf_line *head, const char *dns);
extern const char *resolvconf_get_search_list(struct resolvconf_line *head);
extern int resolvconf_set_search_list(struct resolvconf_line *head, const char *list);
extern struct resolvconf_line *resolvconf_get_next_dns(struct resolvconf_line *line, const char **dns);

static inline int resolvconf_load(struct resolvconf_line *head)
{
	return resolvconf_load__(head, RESOLVCONF_PATH);
}

static inline int resolvconf_save(struct resolvconf_line *head)
{
	return resolvconf_save__(head, RESOLVCONF_PATH);
}

#endif
