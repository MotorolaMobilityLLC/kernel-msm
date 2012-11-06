/*
 * include/linux/vib-timed.h
 *
 * Copyright (C) 2011 Motorola Mobility Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef __VIB_TIMED_H
#define __VIB_TIMED_H

#include <linux/types.h>
#include <linux/sched.h>

#define VIB_TIMED_NAME "vib-timed"

struct vib_timed {
	int (*init) (void *);
	void (*exit) (void *);
	int (*power_on) (int, void *);
	int (*power_off) (void *);
	void (*dump) (void *);
	const char *name;
	unsigned int min_timeout;
	unsigned int max_timeout;
	void *dev_data;
	void *drv_data;
};

struct vib_timed_platform_data {
	struct vib_timed *vibs;
	size_t count;
};

extern int vib_timed_debug;
extern int vib_print(const char *fmt, ...);

#define zprintk(fmt, args...)  \
	do { \
		printk(KERN_INFO fmt, ##args); \
	} while (0)

#define zfprintk(fmt, args...)  \
	do { \
		printk(KERN_INFO "%s "fmt, __func__, ##args); \
	} while (0)

#define dprintk(fmt, args...)  \
	do { \
		if (vib_timed_debug) \
			printk(KERN_INFO "%s "fmt, __func__, ##args); \
	} while (0)

#define vib_tprint(fmt, args...) \
	do { \
		vib_print("%lld %d\t" fmt, ktime_to_us(ktime_get()), \
			current->tgid, ##args); \
	} while (0)

#define zvib_print(fmt, args...)  \
	do { \
		vib_print(fmt, ##args); \
	} while (0)


#define dvib_print(fmt, args...)  \
	do { \
		if (vib_timed_debug) \
			vib_print(fmt, ##args); \
	} while (0)

#define dvib_tprint(fmt, args...)  \
	do { \
		if (vib_timed_debug) \
			vib_tprint(fmt, ##args); \
	} while (0)

#endif /* __VIB_TIMED_H */
