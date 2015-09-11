/*
  * Copyright (c) 2015, Motorola Mobility LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     - Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     - Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     - Neither the name of Motorola Mobility nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA MOBILITY LLC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _UAPI_CONTROL_ACCESS_BLOCK_H_
#define _UAPI_CONTROL_ACCESS_BLOCK_H_

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

extern struct touch_control_access_block *control_access_block_get(void);
#endif
