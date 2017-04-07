/*
 * Copyright (c) 2013-2016 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MC_MAIN_H_
#define _MC_MAIN_H_

#include <linux/device.h>	/* dev_* macros */
#include <linux/slab.h>		/* gfp_t */
#include <linux/fs.h>		/* struct inode and struct file */

#define MC_VERSION(major, minor) \
		(((major & 0x0000ffff) << 16) | (minor & 0x0000ffff))
#define MC_VERSION_MAJOR(x) ((x) >> 16)
#define MC_VERSION_MINOR(x) ((x) & 0xffff)

#define mc_dev_err(fmt, ...) \
	dev_err(g_ctx.mcd, "%s: " fmt "\n", __func__, ##__VA_ARGS__)

#define mc_dev_info(fmt, ...) \
	dev_info(g_ctx.mcd, "%s: " fmt "\n", __func__, ##__VA_ARGS__)

#ifdef DEBUG
#define mc_dev_devel(fmt, ...) \
	dev_info(g_ctx.mcd, "%s: " fmt "\n", __func__, ##__VA_ARGS__)
#else /* DEBUG */
#define mc_dev_devel(...)		do {} while (0)
#endif /* !DEBUG */

#define TEEC_TT_LOGIN_KERNEL	0x80000000

/* MobiCore Driver Kernel Module context data. */
struct mc_device_ctx {
	struct device		*mcd;
	/* debugfs root */
	struct dentry		*debug_dir;

	/* Features */
	/* - SWd uses LPAE MMU table format */
	bool			f_lpae;
	/* - SWd can set a time out to get scheduled at a future time */
	bool			f_timeout;
	/* - SWd supports memory extension which allows for bigger TAs */
	bool			f_mem_ext;
	/* - SWd supports TA authorisation */
	bool			f_ta_auth;
	/* - SWd can map several buffers at once */
	bool			f_multimap;
	/* - SWd supports GP client authentication */
	bool			f_client_login;
	/* - SWd needs time updates */
	bool			f_time;
	/* - SWd supports inter-world protocol */
	bool			f_iwp;
	/* - SWd needs both wall and monotonic times */
	bool			f_monotonic_time;

	/* Debug counters */
	atomic_t		c_clients;
	atomic_t		c_cbufs;
	atomic_t		c_cwsms;
	atomic_t		c_sessions;
	atomic_t		c_wsms;
	atomic_t		c_mmus;
	atomic_t		c_maps;
	atomic_t		c_slots;
};

extern struct mc_device_ctx g_ctx;

/* Debug stuff */
struct kasnprintf_buf {
	gfp_t gfp;
	void *buf;
	int size;
	int off;
};

extern __printf(2, 3)
int kasnprintf(struct kasnprintf_buf *buf, const char *fmt, ...);
ssize_t debug_generic_read(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos,
			   int (*function)(struct kasnprintf_buf *buf));
int debug_generic_release(struct inode *inode, struct file *file);

static inline int kref_read(struct kref *kref)
{
	return atomic_read(&kref->refcount);
}

#endif /* _MC_MAIN_H_ */
