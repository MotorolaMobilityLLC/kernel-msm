/*
 * sfi_processor.h
 * Copyright (c) 2010, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef __SFI_PROCESSOR_H__
#define __SFI_PROCESSOR_H__

#include <linux/sfi.h>
#include <linux/cpuidle.h>

struct sfi_processor_power {
	struct cpuidle_device dev;
	u32 default_state;
	int count;
	struct cpuidle_state *states;
	struct sfi_cstate_table_entry *sfi_cstates;
};

struct sfi_processor_flags {
	u8 valid;
	u8 power;
};

struct sfi_processor {
	u32 id;
	struct sfi_processor_flags flags;
	struct sfi_processor_power power;
	struct sfi_processor_performance *performance;
};

/* Performance management */
struct sfi_processor_px {
	u32 core_frequency;	/* megahertz */
	u32 transition_latency;	/* microseconds */
	u32 control;	/* control value */
};

struct sfi_processor_performance {
	unsigned int state;
	unsigned int state_count;
	struct sfi_processor_px *states;
};

/* for communication between multiple parts of the processor kernel module */
DECLARE_PER_CPU(struct sfi_processor *, sfi_processors);

int sfi_processor_power_init(struct sfi_processor *pr);
int sfi_processor_power_exit(struct sfi_processor *pr);

#endif /*__SFI_PROCESSOR_H__*/
