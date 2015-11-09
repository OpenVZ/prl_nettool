/*
 * Copyright (c) 2015 Parallels IP Holdings GmbH
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
 * Our contact details: Parallels IP Holdings GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 *
 * Implement list of names
 */

#include "common.h"
#include "namelist.h"

int  namelist_add(const char * name, struct namelist **info)
{
	struct namelist *it, *new_it ;

	new_it =(struct namelist *) malloc ( sizeof(struct namelist) ) ;
	if (new_it == NULL)
		goto no_memory;
	(new_it)->name = strdup (  name ) ;
	if ((new_it)->name == NULL) {
		free(new_it);
		new_it = NULL;
		goto no_memory;
	}
	(new_it)->next = NULL ;

	//add to list
	if ((*info) == NULL)
		*info = new_it;
	else {
		//search last
		it = *info ;
		while(it->next) {
			it = it->next;
		}
		it->next = new_it;
	}
	return 0;
no_memory:
	error(errno, "Can't allocate memory");
	return -1;
}

/*search string in info with same name*/
/* return 1 - found */
int  namelist_search(const char *name, struct namelist **info)
{
	struct namelist *it ;
	it = *info ;

	while ( it != NULL )
	{
		if (!strcasecmp(name,it->name))
			return 1 ;
		it = it->next ;
	}

	return 0;
}

int namelist_remove(const char *name, struct namelist **info)
{
	struct namelist *it, *prev = NULL;
	it = *info ;

	while (it != NULL )
	{
		if (!strcasecmp(name, it->name))
		{
			if (prev != NULL)
				prev->next = it->next;
			else
				*info = it->next;

			free(it->name);
			free(it);
			return 0 ;
		}
		prev = it;
		it = it->next ;
	}

	return 1;
}

int namelist_count(struct namelist **info)
{
	int count = 0;
	struct namelist *it ;
	it = *info ;


	while ( it != NULL )
	{
		count ++;
		it = it->next ;
	}

	return count;
}

void  namelist_clean(struct namelist **info)
{
	struct namelist *it , *fit ;
	if (info == NULL)
		return;
	it = *info ;

	while ( it != NULL )
	{
		fit = it ;
		it = it->next ;
		free(fit->name);
		free(fit);
	}

	*info = NULL ;
}

void print_namelist_to_str_delim(struct namelist **info, char *str, size_t size, const char *delim)
{
	struct namelist *it = NULL;

	if (info == NULL || str == NULL)
		return;
	str[0] = '\0';

	it = *info;
	while (it != NULL){
		if (it->name) {
			size_t len = strlen(str);
			if (strlen(it->name) + len >= size-1) {
				error(0, "WARNING: namelist is too large");
				break;
			}
			snprintf(str + len, size - len, "%s%s", it->name, delim);
		}
		it = it->next;
	}
}

int namelist_compare(struct namelist **info, const char *str, const char *delim)
{
	struct namelist *it = NULL;
	char * tmp, *s;

	if (info == NULL || str == NULL)
		return (info == NULL && str == NULL);

	tmp = strdup(str);
	if (tmp == NULL) {
		error(0, "ERROR: failed to strdup");
		return 0;
	}

	for (s = strtok(tmp, delim); s != NULL; s = strtok(NULL, delim)) {
		if (strcasestr(s, "remove") != NULL)
			continue;
		if (!namelist_search(s, info)) {
			free(tmp);
			return 0;
		}
	}

	free(tmp);

	it = *info;
	while (it != NULL) {
		if (it->name && strcasestr(str, it->name) == NULL)
			return 0;
		it = it->next;
	}

	return 1;
}

void print_namelist_to_str(struct namelist **info, char *str, size_t size)
{
	print_namelist_to_str_delim(info, str, size, " ");
}

void print_namelist(struct namelist **info)
{
	char str[1024];
	print_namelist_to_str(info, str, sizeof(str));
	printf("%s", str);
}

void namelist_split_delim(struct namelist **list, const char *str, const char *delim)
{
	char * tmp;
	char * parser;

	if (list == NULL)
		return;

	if (str == NULL || strlen(str) == 0)
		return;

	tmp = strdup(str);
	if (tmp == NULL) {
		error(0, "ERROR: failed to strdup");
		return;
	}

	parser=strtok(tmp, delim);

	while(parser) {
		namelist_add(parser, list);
		parser=strtok(NULL, delim);
	}

	free(tmp);
}

void namelist_split(struct namelist **list, const char *str)
{
	namelist_split_delim(list, str, " ");
}


