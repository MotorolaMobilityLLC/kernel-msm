/* include/linux/partialresume.h
 *
 * Copyright (C) 2015 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_PARTIALRESUME_H
#define _LINUX_PARTIALRESUME_H

#ifdef CONFIG_PARTIALRESUME

#include <linux/list.h>

struct partial_resume_stats {
	unsigned total;
	unsigned total_yes;
};

struct partial_resume {
	struct list_head next_handler;
	struct list_head next_match;
	int irq;
	struct partial_resume_stats stats;
	bool (*partial_resume)(struct partial_resume *);
};

int register_partial_resume(struct partial_resume *handler);
void unregister_partial_resume(struct partial_resume *handler);

bool suspend_again_match(const struct list_head *irqs,
			 const struct list_head *unfinished);
bool suspend_again_consensus(void);

#else /* !CONFIG_PARTIALRESUME */

struct partial_resume;
static inline int register_partial_resume(struct partial_resume *handler) { return 0; }
static inline void unregister_partial_resume(struct partial_resume *handler) {}
static inline bool suspend_again_match(const struct list_head *irqs, size_t irqs_len) { return false; }
static inline bool suspend_again_consensus(void) { return false; }

#endif

#endif

