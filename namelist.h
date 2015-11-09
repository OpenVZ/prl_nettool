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

#ifndef __NAMELIST_H__
#define __NAMELIST_H__


struct namelist
{
	char *name ;
	struct namelist *next ;
};

int  namelist_add(const char * name, struct namelist **info) ;

/*search string in info with same name*/
/* return 1 - found */
int  namelist_search(const char *name, struct namelist **info);

int namelist_count(struct namelist **info);

int namelist_remove(const char *name, struct namelist **info);

void  namelist_clean(struct namelist **info);

void print_namelist(struct namelist **info);

void print_namelist_to_str_delim(struct namelist **info, char *str, size_t size, const char *delim);

void print_namelist_to_str(struct namelist **info, char *str, size_t size);

void namelist_split_delim(struct namelist **list, const char *str, const char *delim);

void namelist_split(struct namelist **list, const char *str);

int namelist_compare(struct namelist **info, const char *str, const char *delim);

#endif
