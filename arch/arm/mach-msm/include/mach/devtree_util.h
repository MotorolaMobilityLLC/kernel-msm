/*
 * Copyright (C) 2012 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __DEVTREE_UTIL_H__
#define __DEVTREE_UTIL_H__

#include <linux/of.h>
#include <asm/mach-types.h>

__init u8  dt_get_u8_or_die(struct device_node *node, const char *name);
__init u16 dt_get_u16_or_die(struct device_node *node, const char *name);
__init u32 dt_get_u32_or_die(struct device_node *node, const char *name);

/* return the number of children for this node */
__init unsigned int dt_children_count(struct device_node *node);
#endif

