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

#include "rcprl.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define RC_PATH		"/etc/rc.conf"

#define RC_HEADER_FIRST	"### The section below was created automatically by prl_nettool. ###\n"
#define RC_HEADER_REST	"### Don't change any line of the section and don't put any      ###\n" \
						"### variables below the section.                                ###\n"
#define RC_BODY			"if [ -r "  RCPRL_PATH     " ]; then                             ###\n" \
						"    . "    RCPRL_PATH   "                                       ###\n" \
						"fi                                                              ###\n"
#define RC_END			"###################################################################\n"

static struct rcconf cfg;

int rcprl_init(void)
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	rcconf_init(&cfg);

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

void rcprl_free(void)
{
	rcconf_free(&cfg);
}

int rcprl_load(void)
{
	return rcconf_load(&cfg, RCPRL_PATH);
}

int rcprl_save(void)
{
	return rcconf_save(&cfg, RCPRL_PATH, RCPRL_HEADER);
}

int rcprl_sublist_load(struct rcconf_sublist *sublist)
{
	return rcconf_sublist_load(sublist, &cfg);
}

int rcprl_sublist_save(struct rcconf_sublist *sublist)
{
	return rcconf_sublist_save(sublist, &cfg);
}
