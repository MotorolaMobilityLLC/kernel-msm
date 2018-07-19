/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 * Based on TDD v7.0 implemented by Mstar & ILITEK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef __PARSER_H
#define __PARSER_H
#include "config.h"

#define BENCHMARK_KEY_NAME "Benchmark_Data"
#define NODE_TYPE_KEY_NAME "node_type_Data"
#define TYPE_MARK "[Driver Type Mark]"
#define VALUE 0

extern void core_parser_nodetype(int32_t *type_ptr, char *desp);
extern void core_parser_benchmark(int32_t *max_ptr, int32_t *min_ptr, int8_t type, char *desp);
extern int core_parser_get_u8_array(char *key, uint8_t *buf);
extern int core_parser_get_int_data(char *section, char *keyname, char *rv);
extern int core_parser_path(char *path);

#endif
