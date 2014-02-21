/* include/linux/quickwakeup.h
 *
 * Copyright (C) 2014 Motorola Mobility LLC.
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

#ifndef _QUICKWAKEUP_H_
#define _QUICKWAKEUP_H_

#ifdef __KERNEL__

struct quickwakeup_ops {
	struct list_head list;
	char *name;
	int (*qw_execute)(void *data);
	int (*qw_check)(void *data);
	int execute;
	void *data;  /* arbitrary data passed back to user */
};

#ifdef CONFIG_QUICK_WAKEUP

int quickwakeup_register(struct quickwakeup_ops *ops);
void quickwakeup_unregister(struct quickwakeup_ops *ops);
bool quickwakeup_suspend_again(void);

#else

static inline int quickwakeup_register(struct quickwakeup_ops *ops) { return 0; };
static inline void quickwakeup_unregister(struct quickwakeup_ops *ops) {};
static inline bool quickwakeup_suspend_again(void) { return 0; };

#endif /* CONFIG_QUICK_WAKEUP */

#endif /* __KERNEL__ */

#endif /* _QUICKWAKEUP_H_ */
