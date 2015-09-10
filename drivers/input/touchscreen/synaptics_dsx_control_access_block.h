/*
 * Copyright (C) 2010-2013 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _SYNAPTICS_DSX_CONTROL_ACCESS_BLOCK_H_
#define _SYNAPTICS_DSX_CONTROL_ACCESS_BLOCK_H_

#include <uapi/linux/control_access_block.h>

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_CTRL_ACCESS_BLK)
struct touch_control_access_block *control_access_block_get(void);
int control_access_block_update_ro(unsigned char gid, unsigned short paddr,
	unsigned char sz, unsigned char cnt);
int control_access_block_update_wo(unsigned char gid, unsigned short paddr,
	unsigned char sz, unsigned char mask, unsigned char *data);
void control_access_block_zap(unsigned char gid);
#else
static inline struct touch_control_access_block *control_access_block_get(void) {
	return NULL;
}
static int inline control_access_block_update_ro(
	unsigned char gid, unsigned short paddr,
	unsigned char sz, unsigned char cnt) {
	return -1;
}
static int inline control_access_block_update_wo(
	unsigned char gid, unsigned short paddr,
	unsigned char sz, unsigned char mask, unsigned char *data) {
	return -1;
}
static void inline control_access_block_zap(unsigned char gid) {
}
#endif

#define control_access_block_update_status(paddr, size) \
	control_access_block_update_ro(SYN_DSX_STATUS, paddr, size, 1)
#define control_access_block_update_mode(paddr, size) \
	control_access_block_update_ro(SYN_DSX_MODE, paddr, size, 1)
#define control_access_block_update_data(paddr, size) \
	control_access_block_update_ro(SYN_DSX_DATA, paddr, size, 1)
#define control_access_block_update_data_mt(paddr, size, mt) \
	control_access_block_update_ro(SYN_DSX_DATA, paddr, size, mt)
#define control_access_block_update_reset(paddr, size, data) \
	control_access_block_update_wo(SYN_DSX_RESET, paddr, size, 0, data)
#define control_access_block_update_wake(paddr, size, data) \
	control_access_block_update_wo(SYN_DSX_WAKE, paddr, size, 0x7, data)
#define control_access_block_update_sleep(paddr, size, data) \
	control_access_block_update_wo(SYN_DSX_SLEEP, paddr, size, 0x7, data)

#endif
