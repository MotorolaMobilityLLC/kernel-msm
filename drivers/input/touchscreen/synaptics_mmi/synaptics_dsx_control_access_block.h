/*
 * Synaptics DSX touchscreen driver extension
 *
 * Copyright (C) 2015 Motorola Mobility, Inc.
 *
 * Copyright (C) 2015 Konstantin Makariev <kmakariev@motorola.com>
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
 */
#ifndef _SYNAPTICS_DSX_CONTROL_ACCESS_BLOCK_H_
#define _SYNAPTICS_DSX_CONTROL_ACCESS_BLOCK_H_

#define MODE_WO	(1 << 7)
#define	GROUP_MAX 7

enum access_groups {
	SYN_DSX_STATUS = 1,	/* RO:mandatory, read status to clear IRQ */
	SYN_DSX_MODE,	/* RO:mandatory, read touch IC mode */
	SYN_DSX_DATA,	/* RO:mandatory, read touch data */
	SYN_DSX_WAKE = SYN_DSX_DATA + 1 + MODE_WO,
			/* WO:mandatory, command(s) to wake up touch IC */
	SYN_DSX_SLEEP,	/* WO:mandatory, command(s) to put touch IC to sleep */
	SYN_DSX_RESET,	/* WO:optional,  command to reset touch IC */
	SYN_DSX_CONFIG,	/* WO:optional,  command(s) to apply custom config */
	SYN_DSX_VAL_MAX = 0xff
};

struct op_read {
	struct {
		unsigned short paddr;	/* (page << 8) + address */
		unsigned char  blk_sz;	/* read block size */
		unsigned char  blk_cnt;	/* read block count */
	};
};

struct op_write {
	struct {
		unsigned short paddr;	/* (page << 8) + address */
		unsigned char  blk_sz;	/* write block size */
		unsigned char  blk_cnt;	/* write block count */
	};
	unsigned char mask;	/* mask to clear bit(s) before apply data */
				/* only effective when blk_cnt and blk_sz */
				/* both are 1s, otherwise mask is ignored */
	unsigned char data[0];	/* blk_cnt of data chunks of blk_sz each */
};

struct grp_info {
	unsigned char group;		/* access group; defines type of data */
	unsigned char data_blk_cnt;	/* number of data blocks in the group */
	union {
		struct op_read **ro_data;
		struct op_write **wo_data;
	};
};

struct touch_control_access_block {
	unsigned char do_sync;	/* flag to indicate data block was updated */
	unsigned char grp_num;	/* number of groups in data block */
	struct grp_info *grp_ptr[GROUP_MAX];
};

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
