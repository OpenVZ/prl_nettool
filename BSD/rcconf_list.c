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

#include "rcconf_list.h"

void rcconf_list_init(struct rcconf_list *list)
{
	list->prev = list;
	list->next = list;
}

void rcconf_list_append(struct rcconf_list *list, struct rcconf_list *item)
{
	item->prev = list->prev;
	item->next = list;
	list->prev->next = item;
	list->prev = item;
}

void rcconf_list_del(struct rcconf_list *item)
{
	item->prev->next = item->next;
	item->next->prev = item->prev;
}
