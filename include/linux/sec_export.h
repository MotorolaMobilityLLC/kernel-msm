#ifndef _LINUX_SEC_EXPORT_H
#define _LINUX_SEC_EXPORT_H
/*
 * Copyright (C) 2009 Motorola, Inc.
 * Copyright (C) 2013 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifdef CONFIG_SEC_DRIVER
void print_hab_fail_codes(void);
#else
static inline void print_hab_fail_codes(void) { }
#endif

#endif
