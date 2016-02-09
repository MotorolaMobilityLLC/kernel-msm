/*
 * Copyright (C) 2014 Motorola Mobility, LLC
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

#ifndef __MMI_HALL_NOTIFIER_H__
#define __MMI_HALL_NOTIFIER_H__

#include <linux/notifier.h>

/* Hall effect sensor type is a position of the */
/* bit representing its state. For example, */
/*	MMI_HALL_FOLIO  0 */
/*	MMI_HALL_SECOND 1 */
/*	MMI_HALL_THIRD  2 */
#define MMI_HALL_FOLIO 0
#define MMI_HALL_MAX   8

struct mmi_hall_data {
	unsigned int enabled;
	unsigned int state;
	struct blocking_notifier_head nhead[MMI_HALL_MAX];
};

#ifdef CONFIG_MMI_HALL_NOTIFICATIONS
struct mmi_hall_data *mmi_hall_init(void);
void mmi_hall_free(struct mmi_hall_data *data);
int mmi_hall_register_notifier(struct notifier_block *nb,
		unsigned long type, bool report);
int mmi_hall_unregister_notifier(struct notifier_block *nb,
		unsigned long type);
void mmi_hall_notify(unsigned long type, int data);
#else
#include <linux/errno.h>
static inline struct mmi_hall_data *mmi_hall_init(void)
{
	return NULL;
}
static inline void mmi_hall_free(struct mmi_hall_data *data) {}
static inline int mmi_hall_register_notifier(struct notifier_block *nb,
		unsigned long type, bool report)
{
	return -ENOSYS;
}
static inline int mmi_hall_unregister_notifier(struct notifier_block *nb,
		unsigned long type)
{
	return -ENOSYS;
}
static inline void mmi_hall_notify(unsigned long type, int data) {}
#endif
#endif

