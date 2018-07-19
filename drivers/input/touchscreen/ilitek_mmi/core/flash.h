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

#ifndef __FLASH_H
#define __FLASH_H

struct flash_table {
uint16_t mid;
uint16_t dev_id;
int mem_size;
int program_page;
int sector;
int block;
};

extern struct flash_table *flashtab;

extern int core_flash_poll_busy(void);
extern int core_flash_write_enable(void);
extern void core_flash_enable_protect(bool status);
extern void core_flash_init(uint16_t mid, uint16_t did);
extern void core_flash_remove(void);

#endif /* __FLASH_H */
